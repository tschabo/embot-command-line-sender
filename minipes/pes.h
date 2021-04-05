#ifndef PES_H
#define PES_H

#include <vector>
#include <filesystem>
#include <limits>

struct color {
	const std::string name;
	const unsigned char r,g,b;
};

struct stitch {
	int x{}, y{}, jumpstitch{}, speed{};
};

struct pes_block {
	color &block_color;
	std::vector<stitch> stitches;
};

struct pes {
	int min_x{std::numeric_limits<int>::max()}, max_x{std::numeric_limits<int>::min()}, min_y{std::numeric_limits<int>::max()}, max_y{std::numeric_limits<int>::min()};
	std::vector<std::reference_wrapper<color>> colors;
	std::vector<pes_block> blocks;
};

/* Input */
std::vector<unsigned char> read_file(const std::filesystem::path& path);
pes parse_pes(const std::vector<unsigned char>& pesBin);

#endif /* PES_H */
