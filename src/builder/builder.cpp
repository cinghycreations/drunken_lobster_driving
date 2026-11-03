#include "gamelib.hpp"
#include <CLI/CLI.hpp>
#include <SDL3_image/SDL_image.h>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fmt/ostream.h>
#include <fmt/printf.h>
#include <fmt/std.h>
#include <fstream>
#include <game.fb.h>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class BuildException : public std::exception {
public:
	explicit BuildException(std::string message) : message(std::move(message)) {}

	const char* what() const noexcept override {
		return message.c_str();
	}

private:
	std::string message;
};

static std::string ninjaEscape(const std::filesystem::path& path) {
	std::stringstream ss;
	for (const char c : path.string()) {
		if (c == ' ' || c == '$' || c == ':') {
			ss << '$' << c;
		} else {
			ss << c;
		}
	}
	return ss.str();
}

static void generate() {
	const std::filesystem::path windows = "windows";
	const std::filesystem::path emscripten = "emscripten";

	std::ofstream ninja("build.ninja");
	std::vector<std::filesystem::path> windows_outs;
	std::vector<std::filesystem::path> emscripten_outs;

	fmt::println(ninja, "rule copy\n  command = cmake -E copy $in $out");
	fmt::println(ninja, "rule copy_filter\n  command = builder copyfilter --in $in --out $out");
	fmt::println(ninja, "rule image_windows\n  command = builder image --webp --in $in --out $out");
	fmt::println(ninja, "rule image_emscripten\n  command = builder image --webp --in $in --out $out");
	fmt::println(ninja, "");

	for (const auto path_iter : std::filesystem::recursive_directory_iterator("source")) {
		if (path_iter.is_directory()) {
			continue;
		}

		const std::filesystem::path in = path_iter.path();
		const std::filesystem::path relative = path_iter.path().lexically_relative("source");
		const std::filesystem::path extension = path_iter.path().extension();

		if (extension == std::filesystem::path(".png")) {
			const std::filesystem::path windows_out = (windows / relative).replace_extension(".image");
			const std::filesystem::path emscripten_out = (emscripten / relative).replace_extension(".image");
			windows_outs.push_back(windows_out);
			emscripten_outs.push_back(emscripten_out);

			fmt::println(ninja, "build {}: image_windows {}", ninjaEscape(windows_out), ninjaEscape(in));
			fmt::println(ninja, "build {}: image_emscripten {}", ninjaEscape(emscripten_out), ninjaEscape(in));
		} else if (extension == std::filesystem::path(".html") || extension == std::filesystem::path(".css") || extension == std::filesystem::path(".tmj")) {
			const std::filesystem::path windows_out = windows / relative;
			const std::filesystem::path emscripten_out = emscripten / relative;
			windows_outs.push_back(windows_out);
			emscripten_outs.push_back(emscripten_out);

			fmt::println(ninja, "build {}: copy_filter {}", ninjaEscape(windows_out), ninjaEscape(in));
			fmt::println(ninja, "build {}: copy_filter {}", ninjaEscape(emscripten_out), ninjaEscape(in));
		} else {
			const std::filesystem::path windows_out = windows / relative;
			const std::filesystem::path emscripten_out = emscripten / relative;
			windows_outs.push_back(windows_out);
			emscripten_outs.push_back(emscripten_out);

			fmt::println(ninja, "build {}: copy {}", ninjaEscape(windows_out), ninjaEscape(in));
			fmt::println(ninja, "build {}: copy {}", ninjaEscape(emscripten_out), ninjaEscape(in));
		}
	}

	fmt::print(ninja, "build windows: phony");
	for (const std::filesystem::path& windows_out : windows_outs) {
		fmt::print(ninja, " {}", ninjaEscape(windows_out));
	}
	fmt::println(ninja, "");

	fmt::print(ninja, "build emscripten: phony");
	for (const std::filesystem::path& emscripten_out : emscripten_outs) {
		fmt::print(ninja, " {}", ninjaEscape(emscripten_out));
	}
	fmt::println(ninja, "");
}

static void copyFilter(const std::filesystem::path& in, const std::filesystem::path& out) {
	std::ifstream in_stream(in, std::ios::binary);
	if (!in_stream) {
		throw BuildException(fmt::format("Failed to load file {}", in));
	}
	std::string data((std::istreambuf_iterator<char>(in_stream)), std::istreambuf_iterator<char>());

	constexpr std::string_view from = ".png";
	constexpr std::string_view to = ".image";
	for (size_t pos = data.find(from); pos != std::string::npos; pos = data.find(from, pos + to.size())) {
		data.replace(pos, from.size(), to);
	}

	std::ofstream out_stream(out, std::ios::binary);
	out_stream.write(data.data(), data.size());
	if (!out_stream) {
		throw BuildException(fmt::format("Failed to save file {}", out));
	}
}

static void image(const std::filesystem::path& in, const std::filesystem::path& out, const bool webp) {

	flatbuffers::FlatBufferBuilder builder;

	if (!webp) {
		size_t data_size = 0;
		void* data = SDL_LoadFile(reinterpret_cast<const char*>(in.u8string().c_str()), &data_size);
		if (!data) {
			throw BuildException(fmt::format("Failed to load image {}: {}", in, SDL_GetError()));
		}

		flatbuffers::Offset<flatbuffers::Vector<uint8_t>> image_data = builder.CreateVector(reinterpret_cast<const uint8_t*>(data), data_size);

		data::ImageBuilder image_builder(builder);
		image_builder.add_type(data::ImageType_Png);
		image_builder.add_data(image_data);
		const flatbuffers::Offset<data::Image> image = image_builder.Finish();
		data::FinishImageBuffer(builder, image);

		SDL_free(data);
	} else {
		SDL_Surface* surface = IMG_Load(reinterpret_cast<const char*>(in.u8string().c_str()));
		if (!surface) {
			throw BuildException(fmt::format("Failed to load image {}: {}", in, SDL_GetError()));
		}

		SDL_IOStream* stream = SDL_IOFromDynamicMem();
		if (!stream) {
			SDL_DestroySurface(surface);
			throw BuildException(fmt::format("Failed to create memory stream: {}", SDL_GetError()));
		}

		const bool saved = IMG_SaveWEBP_IO(surface, stream, false, 95);
		SDL_DestroySurface(surface);
		if (!saved) {
			SDL_CloseIO(stream);
			throw BuildException(fmt::format("Failed to encode image {}: {}", in, SDL_GetError()));
		}

		const uint8_t* data = static_cast<const uint8_t*>(SDL_GetPointerProperty(SDL_GetIOProperties(stream), SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, nullptr));
		const Sint64 data_size = SDL_GetIOSize(stream);

		flatbuffers::Offset<flatbuffers::Vector<uint8_t>> image_data = builder.CreateVector(reinterpret_cast<const uint8_t*>(data), data_size);
		data::ImageBuilder image_builder(builder);
		image_builder.add_type(data::ImageType_Webp);
		image_builder.add_data(image_data);
		const flatbuffers::Offset<data::Image> image = image_builder.Finish();
		data::FinishImageBuffer(builder, image);

		SDL_CloseIO(stream);
	}

	std::ofstream out_stream(out, std::ios::binary);
	out_stream.write(reinterpret_cast<const char*>(builder.GetBufferPointer()), builder.GetSize());
	if (!out_stream) {
		throw BuildException(fmt::format("Failed to save image {}", out));
	}
}

int main(int argc, char* argv[]) {
	CLI::App cli;
	cli.require_subcommand();

	CLI::App* generate_cmd = cli.add_subcommand("generate", "Generate build file");

	CLI::App* copy_filter_cmd = cli.add_subcommand("copyfilter", "Copy file, replacing image extensions");
	std::string copy_filter_in;
	std::string copy_filter_out;
	copy_filter_cmd->add_option("--in", copy_filter_in)->required();
	copy_filter_cmd->add_option("--out", copy_filter_out)->required();

	CLI::App* image_cmd = cli.add_subcommand("image", "Build image");
	bool image_webp = false;
	std::string image_in;
	std::string image_out;
	image_cmd->add_flag("--webp", image_webp);
	image_cmd->add_option("--in", image_in)->required();
	image_cmd->add_option("--out", image_out)->required();

	try {
		cli.parse(argc, argv);
	} catch (const CLI::ParseError& e) {
		return cli.exit(e);
	}

	try {
		if (generate_cmd->parsed()) {
			generate();
		} else if (copy_filter_cmd->parsed()) {
			copyFilter(copy_filter_in, copy_filter_out);
		} else if (image_cmd->parsed()) {
			image(image_in, image_out, image_webp);
		}
	} catch (const BuildException& e) {
		fmt::println(stderr, "{}", e.what());
		return EXIT_FAILURE;
	}

	return 0;
}
