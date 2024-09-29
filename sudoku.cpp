#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>

const int SIZE = 9;

bool isSafe(const std::vector<std::vector<int>>& board, int row, int col, int num) {
    for (int x = 0; x < SIZE; x++) {
        if (board[row][x] == num || board[x][col] == num) return false;
    }
    int startRow = row - row % 3, startCol = col - col % 3;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (board[i + startRow][j + startCol] == num) return false;
        }
    }
    return true;
}

bool solveSudoku(std::vector<std::vector<int>>& board) {
    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
            if (board[row][col] == 0) {
                for (int num = 1; num <= 9; num++) {
                    if (isSafe(board, row, col, num)) {
                        board[row][col] = num;
                        if (solveSudoku(board)) return true;
                        board[row][col] = 0;
                    }
                }
                return false;
            }
        }
    }
    return true;
}

// Génère un Sudoku complètement résolu
void generateSudoku(std::vector<std::vector<int>>& board) {
    // Remplir les blocs diagonaux 3x3 avec des nombres uniques
    for (int i = 0; i < SIZE; i += 3) {
        std::vector<int> numbers = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        std::random_shuffle(numbers.begin(), numbers.end());
        for (int j = 0; j < 9; j++) {
            board[i + j / 3][i + j % 3] = numbers[j];
        }
    }
    solveSudoku(board);
}

void removeNumbers(std::vector<std::vector<int>>& board, int numToRemove) {
    while (numToRemove > 0) {
        int row = rand() % SIZE;
        int col = rand() % SIZE;
        if (board[row][col] != 0) {
            board[row][col] = 0; // avoid 0 for empty cells
            numToRemove--;
        }
    }
}


void printBoard(const std::vector<std::vector<int>>& board) {
    for (const auto& row : board) {
        for (int num : row) {
            if (num == 0) {
                std::cout << "- ";
            }
            else {
                std::cout << num << " ";
            }
        }
        std::cout << std::endl;
    }
}

bool isValidSudoku(const std::vector<std::vector<int>>& board) {
    // rows
    for (int row = 0; row < SIZE; row++) {
        std::vector<bool> rowCheck(SIZE, false);
        for (int col = 0; col < SIZE; col++) {
            if (board[row][col] != 0) {
                if (rowCheck[board[row][col] - 1]) return false;
                rowCheck[board[row][col] - 1] = true;
            }
        }
    }

    // columns
    for (int col = 0; col < SIZE; col++) {
        std::vector<bool> colCheck(SIZE, false);
        for (int row = 0; row < SIZE; row++) {
            if (board[row][col] != 0) {
                if (colCheck[board[row][col] - 1]) return false;
                colCheck[board[row][col] - 1] = true;
            }
        }
    }

    // inner blocks 3x3
    for (int row = 0; row < SIZE; row += 3) {
        for (int col = 0; col < SIZE; col += 3) {
            std::vector<bool> gridCheck(SIZE, false);
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    int num = board[row + i][col + j];
                    if (num != 0) {
                        if (gridCheck[num - 1]) return false;
                        gridCheck[num - 1] = true;
                    }
                }
            }
        }
    }

    return true; 
}

int main() {
    srand(static_cast<unsigned int>(time(0)));

    const int NUM_SUDOKUS = 10; 

    for (int sudokuCount = 1; sudokuCount <= NUM_SUDOKUS; sudokuCount++) {
        std::vector<std::vector<int>> board(SIZE, std::vector<int>(SIZE, 0));

        generateSudoku(board);

        std::vector<std::vector<int>> solvedBoard = board;

        int clues = 32 + rand() % 5; // 32 -> 36
        int numToRemove = 81 - clues;

        removeNumbers(board, numToRemove);

        std::cout << "\n\n==============================" << std::endl;
        std::cout << "Sudoku #" << sudokuCount << " (Indices existant : " << clues << ")" << std::endl;
        std::cout << "==============================" << std::endl;

        std::cout << "Puzzle :" << std::endl;
        printBoard(board);


        std::cout << "\nSolution :" << std::endl;
        printBoard(solvedBoard);

        std::cout << "check : ";
        if (isValidSudoku(board)) {
            std::cout << "ok" << std::endl;
        }
        else {
            std::cout << "Invalide" << std::endl;
        }

        std::cout << std::endl;
    }

    return 0;
}
