// pms.cpp
// Author: Jan Kalenda
// Year: 2024

// first core just takes the input and sends it to the next core, alternating between his queues
// for the middle cores to start they need to fill their queue1 to 2^rank-1 and queue2 with at least 1 element
// the middle cores connects the elements from the queues until a queue that is twice the length of their queues is created
// connecting is done by comparing first elements of both queues and sending the greater one to the next core, followed by the smaller one
// if a new element is added to the queue, the element is suspended until the previous iteration is finished
// the last core just outputs the result

#include <iostream>
#include <fstream>
#include <queue>
#include <mpi.h>

// macros for easier access to the values in the array that represents the number
// the first element is the destination, more precisely the queue it belongs to
// the second element is the value of the number
#define VALUE(x) *(x + 1)
#define DESTINATION(x) *(x)

// enum for the destination of the number
enum Dest {
    Q1 = 0,
    Q2 = 1,
    END = 2
};

// class that represents the core
class Core {
public:
    int rank = 0;
    int size = 0;
    int des = Q1;
    int sent = 0;
    int to_send = 1 << rank;
    int Q1_sent = 0;
    int Q2_sent = 0;
    bool end_flag = false;
    std::queue<unsigned char*>queue1;
    std::queue<unsigned char*>queue2;

    // Does the comparison of the first elements of the queues and returns the one that should be sent
    unsigned char* order() {
        if (queue1.empty() && queue2.empty()) {
            return nullptr;
        }

        // first core only takes from queue1
        if (rank == 0) {
            unsigned char *number = queue1.front();
            queue1.pop();
            return number;
        }

        // if the end_flag is set, the core is in the last iteration and should just send the leftover elements
        if (end_flag) {
            if (queue1.empty() && !queue2.empty()) {
                unsigned char *number = queue2.front();
                queue2.pop();
                return number;
            }
            if (queue2.empty() && !queue1.empty()) {
                unsigned char *number = queue1.front();
                queue1.pop();
                return number;
            }
        }

        // assures completion of an iteration
        if (Q2_sent == (1 << (rank - 1)) || Q1_sent == (1 << (rank - 1))) {
            if (!queue2.empty() && Q2_sent < (1 << (rank - 1))) {
                unsigned char *number = queue2.front();
                queue2.pop();
                Q2_sent++;
                return number;
            }

            if (!queue1.empty() && Q1_sent < (1 << (rank - 1))) {
                unsigned char *number = queue1.front();
                queue1.pop();
                Q1_sent++;
                return number;
            }
        }
        unsigned char *number1 = queue1.front();
        unsigned char *number2 = queue2.front();
        if (VALUE(number1) < VALUE(number2)) {
            queue1.pop();
            Q1_sent++;
            return number1;
        }
        queue2.pop();
        Q2_sent++;
        return number2;
    }

    // sends the number to the next core
    void send() {
        unsigned char *number = order();
        if (number == nullptr) {
            return;
        }
        // after the iteration is finished, the counters are reset
        if (rank != 0) {
            if (Q1_sent == (1 << (rank - 1)) && Q2_sent == (1 << (rank - 1))) {
                Q1_sent = 0;
                Q2_sent = 0;
            }
        }
        DESTINATION(number) = des;

        //edge case of number of cores == 1
        if (rank == size - 1) {
            std::cout << static_cast<int>(VALUE(number)) << std::endl;
            delete[] number;
        } else {
            MPI_Send(number, 2, MPI_UNSIGNED_CHAR, rank+1, 0, MPI_COMM_WORLD);
            delete[] number;
        }
        sent++;
        // if the number of elements sent is equal to the number of elements to send, the destination is changed
        if (sent == to_send) {
            sent = 0;
            des ^= 1; // change the destination
        }
    }

    // sends a signal to the next core that the last element was sent
    void signal_end() const {
        if (size == 1) {
            return;
        }
        auto *number = new unsigned char[2];
        DESTINATION(number) = END; // end_flag for the last element
        MPI_Send(number, 2, MPI_UNSIGNED_CHAR, rank+1, 0, MPI_COMM_WORLD);
        delete[] number;
    }
};


int main(int argc, char **argv) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    Core core;
    core.rank = rank;
    core.size = size;
    core.to_send = 1 << rank;

    // load all the numbers and print them
    if (rank == 0) {
        // open file numbers
        std::ifstream file("numbers", std::ios::binary | std::ios::in);
        if (!file.is_open()) {
            std::cerr << "File not found" << std::endl;
            return 1;
        }
        // read the numbers and print them and add them to the queue of the first core
        while (!file.eof()) {
            auto *number = new unsigned char[2];
            file.read(reinterpret_cast<char*>(number + 1), sizeof(unsigned char));
            core.queue1.push(number);
            if (file.peek() == EOF) {
                std::cout << static_cast<int>(VALUE(number)) << std::endl;
            } else {
                std::cout << static_cast<int>(VALUE(number)) << " ";
            }
        }
        file.close();
    }

    while (true) {
        // first core just sends the numbers
        if (rank == 0) {
            if (core.queue1.empty() && core.queue2.empty()) {
                core.signal_end();
                break;
            }
            core.send();
        }
        // middle cores need to fill their queues before they start
        else if (rank != size - 1 && rank != 0) {
            if (!core.end_flag) {
                auto *number = new unsigned char[2];
                MPI_Recv(number, 2, MPI_UNSIGNED_CHAR, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                if (DESTINATION(number) == END) { // all the numbers were sent from the previous core
                    core.end_flag = true;
                    delete[] number;
                } else { // the number is added to the queue and the core starts sending if the conditions are met
                    DESTINATION(number) ? core.queue2.push(number) : core.queue1.push(number);
                    if (core.queue1.size() == (1 << (rank - 1)) && !core.queue2.empty()) {
                        core.send();
                    }
                }
            }
            else {
                if (!core.queue1.empty() || !core.queue2.empty()) { // send the remaining numbers
                    core.send();
                }
                else { // all the numbers were sent
                    core.signal_end();
                    break;
                }
            }
        }
        // last core does what the middle cores do, but also prints the result
        else if (rank == size - 1) {
            if (!core.end_flag) {
                auto *number = new unsigned char[2];
                MPI_Recv(number, 2, MPI_UNSIGNED_CHAR, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                if (DESTINATION(number) == END) {
                    core.end_flag = true;
                    delete[] number;
                } else {
                    DESTINATION(number) ? core.queue2.push(number) : core.queue1.push(number);
                    if (core.queue1.size() >= (1 << (rank - 1)) && !core.queue2.empty()) {
                        unsigned char *number = core.order();
                        std::cout << static_cast<int>(VALUE(number)) << std::endl;
                        delete[] number;
                    }
                }
            } else {
                if (!core.queue1.empty() || !core.queue2.empty()) { // print the remaining numbers
                    unsigned char *number = core.order();
                    std::cout << static_cast<int>(VALUE(number)) << std::endl;
                    delete[] number;
                }
                else { // all the numbers were printed
                    break;
                }
            }
        }
    }

    MPI_Finalize();
}
