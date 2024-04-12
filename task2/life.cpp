// life.cpp
// Author: Jan Kalenda
// Year: 2024

//

#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <numeric>
#include <sstream>
#include <mpi.h>

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

int get_neighbour_count(const std::vector<std::string> &lines, const long x, const long y) {
	int count = 0;
	for (int i = -1; i <= 1; i++) {
		for (int j = -1; j <= 1; j++) {
			if (i == 0 && j == 0) {
				continue;
			}

			const long new_x = x + i;
			const size_t normalised_x = new_x < 0 ? lines.size() - 1 : new_x % lines.size();

			const long new_y = y + j;
			const size_t normalised_y = new_y < 0 ? lines[normalised_x].size() + j : new_y % lines[normalised_x].size();
			// int prev = i - 1 ? i - 1 : N;

			if (lines[normalised_x][normalised_y] == '1') {
				count++;
			}
		}
	}

	return count;
}

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

int main(int argc, char **argv) {
	MPI_Init(&argc, &argv);

	int rank, size, iteration_count, matrix_size;
	int first_last[2]; // FIXME - change to std::pair
	std::string new_line; // used to store the result of the core computation
	std::vector<char> MPI_lines;
	std::vector<std::string> lines;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	if (rank == 0) {
		if (argc < 3) {
			std::cerr << "Usage: " << argv[0] << " <input_file> <iteration_count>" << std::endl;
			MPI_Finalize();
			return 1;
		}

		// std::string input_file = argv[1];
		std::ifstream file(argv[1], std::ios::in);
		if (!file.is_open()) {
			std::cerr << "ERR: File not found" << std::endl;
			MPI_Finalize();
			return 1;
		}

		std::string lines_sending;
		for (std::string line; std::getline(file, line);) {
			lines_sending += (line + '\n');
			lines.push_back(line);
		}
		file.close();

		if (lines.empty()) {
			std::cerr << "ERR: Empty file" << std::endl;
			MPI_Finalize();
			return 1;
		}

		int lines_per_core = std::ceil(static_cast<double>(lines.size()) / size);
		int redundant_cores = static_cast<int>(lines_per_core > 1 ? 0 : size - (lines.size() * lines_per_core));
		matrix_size = lines.size() * lines[0].size() + lines.size(); // + lines.size() for newlines

		// lines_sending = std::accumulate(lines.begin(), lines.end(), std::string(),
		// 									[](const std::string& a, const std::string& b) {
		// 										return a + (a.length() > 0 ? "\n" : "") + b;
		// 									});
		MPI_lines = std::vector<char>(lines_sending.begin(), lines_sending.end());

		iteration_count = std::stoi(argv[2]);
		// std::cout << "Iteration count: " << iteration_count << std::endl;
		// std::cout << "Lines per core: " << lines_per_core << std::endl;
		// std::cout << "Redundant cores: " << redundant_cores << std::endl;

		std::vector<int> range(size-1);
		std::iota(range.begin(), range.end(), 1);
		for (const auto i : range) {
			first_last[0] = i * lines_per_core;
			first_last[1] = first_last[0] + lines_per_core - 1;
			if (i >= (size - redundant_cores)) {
				first_last[1] = -1;
			}
			if (i != 0) {
				MPI_Send(first_last, 2, MPI_INT, i, 0, MPI_COMM_WORLD);
			}
		}
		first_last[0] = 0;
		first_last[1] = lines_per_core - 1;

		size -= redundant_cores;
	}

	MPI_Bcast(&iteration_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&matrix_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&size, 1, MPI_INT, 0, MPI_COMM_WORLD);

	MPI_lines.resize(matrix_size);

	if (rank != 0) {
		MPI_Recv(first_last, 2, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		if (first_last[1] == -1) {
			MPI_Finalize();
			return 0;
		}
	}


	// std::cout << "Rank: " << rank << " Iteration count: " << iteration_count << std::endl;
	// std::cout << "Rank: " << rank << " Matrix size: " << matrix_size << std::endl;
	// std::cout << "Rank: " << rank << " Lines:\n" << std::string(MPI_lines.begin(), MPI_lines.end()) << std::endl;
	// std::cout << "==============" << std::endl;

	for (int i = 0; i < iteration_count; i++) {
		new_line.clear(); // nullify the new line for the next iteration
		MPI_Bcast(MPI_lines.data(), matrix_size, MPI_CHAR, 0, MPI_COMM_WORLD);
		lines = split_lines(std::string(MPI_lines.begin(), MPI_lines.end() - 1)); // -1 to remove the string terminator

		std::vector<char> buffer((lines[0].size() + 1) * (first_last[1] - first_last[0] + 1) + 1); // +1 for the string terminator

		// suspected bottleneck
		for (int x = first_last[0]; x <= first_last[1]; x++) {
			for (int y = 0; y < lines[x].size(); y++) {
				new_line += apply_rule(lines[x][y], get_neighbour_count(lines, x, y));
			}
			new_line += '\n';
		}
		if (rank != 0) {
			// send the result back to the master
			MPI_Send(new_line.data(), new_line.size(), MPI_CHAR, 0, 0, MPI_COMM_WORLD);
		} else if (rank == 0) {
			std::vector<int> range(size-1);
			std::iota(range.begin(), range.end(), 1);
			for (const auto i : range) {
				// receive the result from the slaves
				MPI_Recv(buffer.data(), (lines[0].size() + 1) * (first_last[1] - first_last[0] + 1), MPI_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				// concatenate the result
				new_line += std::string(buffer.begin(), buffer.end() - 1);
			}
			// prepare the table for the next iteration
			MPI_lines = std::vector<char>(new_line.begin(), new_line.end());
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

	// std::cout << "Rank: " << rank << " quit"  << std::endl;


	MPI_Finalize();
}
