// life.cpp
// Author: Jan Kalenda
// Year: 2024
// Description: Game of life implementation using MPI
// The program was run with a script utilising 8 cores

#include <array>
#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <numeric>
#include <sstream>
#include <mpi.h>

/// Apply the rules of the game of life
/// @param val value of the cell (1 - alive, 0 - dead)
/// @param count number of alive neighbours
/// @return new value of the cell
std::string apply_rule(const char val, const int count) {
	if (val == '1') {
		switch (count) {
			case 2:
			case 3:
				return "1";
			default:
				return "0";
		}
	}
	// dead cell
	if (count == 3) {
		return "1";
	}
	return "0";
}

/// Get the number of alive neighbours
/// @param lines vector of strings representating the table
/// @param y y coordinate
/// @param x x coordinate
/// @return number of alive neighbours
int get_neighbour_count(const std::vector<std::string> &lines, const long y, const long x) {
	int count = 0;
	for (int i = -1; i <= 1; i++) {
		for (int j = -1; j <= 1; j++) {
			if (i == 0 && j == 0) {
				continue;
			}

			const long new_y = y + i;
			const size_t normalised_y = new_y < 0 ? lines.size() - 1 : new_y % lines.size();

			const long new_x = x + j;
			const size_t normalised_x = new_x < 0 ? lines[normalised_y].size() + j : new_x % lines[normalised_y].size();
			// int prev = i - 1 ? i - 1 : N;

			if (lines[normalised_y][normalised_x] == '1') {
				count++;
			}
		}
	}

	return count;
}

/// Split the string into lines
/// @param lines string to split
/// @return vector of strings
std::vector<std::string> split_lines(const std::string &lines) {
	std::vector<std::string> result;
	std::istringstream line_stream(lines);
	for (std::string line; std::getline(line_stream, line); ) {
		if (line.empty()) {
			continue;
		}
		result.push_back(line);
	}
	return result;
}

/// Generate a range of numbers
/// @tparam T type of the numbers
/// @param size size of the range
/// @param start start of the range
/// @return vector of numbers
template<typename T>
std::vector<T>range (const int size, const T start = 0) {
	std::vector<T> result(size);
	std::iota(result.begin(), result.end(), start);
	return result;
}

int main(int argc, char **argv) {
	MPI_Init(&argc, &argv);

	int rank, size, iteration_count, matrix_size;
	std::pair<int, int> first_last; // used to store the first and last line index for the current core
	std::string new_line; // used to store the result of the core computation
	std::vector<char> MPI_lines; // used to store the lines for the MPI communication
	std::vector<std::string> lines;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	if (rank == 0) {
		if (argc < 3) {
			std::cerr << "Usage: " << argv[0] << " <input_file> <iteration_count>" << std::endl;
			MPI_Abort(MPI_COMM_WORLD, 1);
		}

		try {
			iteration_count = std::stoi(argv[2]);
		} catch (const std::invalid_argument &_) {
			std::cerr << "ERR: Invalid iteration count" << std::endl;
			MPI_Abort(MPI_COMM_WORLD, 1);
		}

		std::ifstream file(argv[1], std::ios::in);
		if (!file.is_open()) {
			std::cerr << "ERR: File not found" << std::endl;
			MPI_Abort(MPI_COMM_WORLD, 1);
		}

		std::string lines_sending;
		for (std::string line; std::getline(file, line);) {
			lines_sending += (line + '\n');
			if (line.empty()) {
				continue;
			}
			lines.push_back(line);
		}
		file.close();

		if (lines.empty()) {
			std::cerr << "ERR: Empty file" << std::endl;
			MPI_Abort(MPI_COMM_WORLD, 1);
		}

		int lines_per_core = std::ceil(static_cast<double>(lines.size()) / size);
		int redundant_cores = static_cast<int>(lines_per_core > 1 ? 0 : size - (lines.size() * lines_per_core));
		matrix_size = static_cast<int>(lines.size() * lines[0].size() + lines.size()); // + lines.size() for newlines
		MPI_lines = std::vector(lines_sending.begin(), lines_sending.end());

		for (const auto i : range(size-1, 1)) {
			first_last = {i * lines_per_core, i * lines_per_core + lines_per_core - 1};
			if (i >= (size - redundant_cores)) {
				first_last.second = -1;
			}
			if (i != 0) {
				MPI_Send(std::array{first_last.first, first_last.second}.data(), 2, MPI_INT, i, 0, MPI_COMM_WORLD);
			}
		}
		first_last = {0, lines_per_core - 1};
		size -= redundant_cores;
	}

	if (rank != 0) {
		int rcv[2];
		MPI_Recv(rcv, 2, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		first_last = std::pair(rcv[0], rcv[1]);

		if (first_last.second == -1) {
			MPI_Finalize();
			return 0;
		}
	}

	MPI_Bcast(&iteration_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&matrix_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&size, 1, MPI_INT, 0, MPI_COMM_WORLD);

	MPI_lines.resize(matrix_size);

	for (int i = 0; i < iteration_count; i++) {
		new_line.clear(); // nullify the new line for the next iteration
		MPI_Bcast(MPI_lines.data(), matrix_size, MPI_CHAR, 0, MPI_COMM_WORLD);
		lines = split_lines(std::string(MPI_lines.begin(), MPI_lines.end() - 1)); // -1 to remove the string terminator

		std::vector<char> buffer((lines[0].size() + 1) * (first_last.second - first_last.first + 1) + 1); // +1 for the string terminator

		// suspected bottleneck
		for (int y = first_last.first; y <= first_last.second; y++) {
			for (int x = 0; x < lines[y].size(); x++) {
				new_line += apply_rule(lines[y][x], get_neighbour_count(lines, y, x));
			}
			new_line += '\n';
		}
		if (rank != 0) {
			// send the result back to the master
			MPI_Send(new_line.data(), static_cast<int>(new_line.size()), MPI_CHAR, 0, 0, MPI_COMM_WORLD);
		} else if (rank == 0) {
			for (const auto j : range(size-1, 1)) {
				// receive the result from the slaves
				MPI_Recv(buffer.data(), static_cast<int>((lines[0].size() + 1) * (first_last.second - first_last.first + 1)), MPI_CHAR, j, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				// concatenate the result
				new_line += std::string(buffer.begin(), buffer.end() - 1);
			}
			// prepare the table for the next iteration
			MPI_lines = std::vector(new_line.begin(), new_line.end());
		}
	}

	if (rank == 0) {
		lines = split_lines(new_line);
		int proc = 0;
		int i = 0;
		for (const auto &line : lines) {
			std::cout << proc << ": " << line << std::endl;
			i++;
			if (i == static_cast<int>(std::ceil(static_cast<double>(lines.size()) / size))) {
				proc++;
				i = 0;
			}
		}
	}

	MPI_Finalize();
}
