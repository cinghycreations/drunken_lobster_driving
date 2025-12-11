let filter = 'fps=15,scale=480:-1:flags=lanczos,split[s0][s1];[s0]palettegen=max_colors=32[p];[s1][p]paletteuse=dither=bayer'
ffmpeg -i movie1.mp4 -t 0:3 -ss 0:4 -filter_complex $filter -y movie1.gif
ffmpeg -i movie2.mp4 -t 0:3 -ss 0:5 -filter_complex $filter -y movie2.gif
