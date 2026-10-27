#include <CLI/CLI.hpp>
#include <SDL3_image/SDL_image.h>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fmt/ostream.h>
#include <fmt/printf.h>
#include <fmt/std.h>
#include <string>
#include <utility>

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
	fmt::println(ninja, "rule image_windows\n  command = builder image --in $in --out $out");
	fmt::println(ninja, "rule image_emscripten\n  command = builder image --webp --in $in --out $out");
	fmt::println(ninja, "");

	for (const auto path_iter : std::filesystem::recursive_directory_iterator("source")) {
		if (path_iter.is_directory()) {
			continue;
		}

		const std::filesystem::path in = path_iter.path();
		const std::filesystem::path relative = path_iter.path().lexically_relative("source");

		if (path_iter.path().extension() == std::filesystem::path(".png")) {
			const std::filesystem::path windows_out = (windows / relative).replace_extension(".image");
			const std::filesystem::path emscripten_out = (emscripten / relative).replace_extension(".image");
			windows_outs.push_back(windows_out);
			emscripten_outs.push_back(emscripten_out);

			fmt::println(ninja, "build {}: image_windows {}", ninjaEscape(windows_out), ninjaEscape(in));
			fmt::println(ninja, "build {}: image_emscripten {}", ninjaEscape(emscripten_out), ninjaEscape(in));
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

static void image(const std::filesystem::path& in, const std::filesystem::path& out, const bool webp) {
	if (!webp) {
		std::filesystem::copy_file(in, out, std::filesystem::copy_options::overwrite_existing);
		return;
	}

	SDL_Surface* surface = IMG_Load(reinterpret_cast<const char*>(in.u8string().c_str()));
	if (!surface) {
		throw BuildException(fmt::format("Failed to load image {}: {}", in, SDL_GetError()));
	}

	const bool saved = IMG_SaveWEBP(surface, reinterpret_cast<const char*>(out.u8string().c_str()), 95);
	SDL_DestroySurface(surface);
	if (!saved) {
		throw BuildException(fmt::format("Failed to save image {}: {}", out, SDL_GetError()));
	}
}

int main(int argc, char* argv[]) {
	CLI::App cli;
	cli.require_subcommand();

	CLI::App* generate_cmd = cli.add_subcommand("generate", "Generate build file");

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
		} else if (image_cmd->parsed()) {
			image(image_in, image_out, image_webp);
		}
	} catch (const BuildException& e) {
		fmt::println(stderr, "{}", e.what());
		return EXIT_FAILURE;
	}

	return 0;
}
