#include <chrono>
#include <iostream>
#include <mutex>
#include <random>
#include <redpoule.hpp>
#include <vector>

const int n_matrices = 1000;
const int dim1 = 50;
const int dim2 = 100;
const int dim3 = 3;

std::random_device rd{};
std::mt19937 rng{rd()};

template <class T>
class Matrix {
  /*
   * We define a matrix class to have something nice to test
   * the library
   */
 private:
  size_t _rows{0}, _cols{0};  // shape information
  std::vector<T> _data;       // matrix content

 public:
  // default constructor
  Matrix(){};
  // constructor providing shape information
  Matrix(size_t r, size_t c) : _rows(r), _cols(c) {
    _data.resize(_rows * _cols, T(0));
  }
  // copy constructor
  Matrix(const Matrix &M) : _rows(M._rows), _cols(M._cols), _data(M._data) {}

  // getter for shape information
  size_t rows() const { return _rows; }
  size_t cols() const { return _cols; }

  // Assignment copy operator
  Matrix &operator=(const Matrix &m) {
    _data = m._data;
    _rows = m._rows;
    _cols = m._cols;
    return *this;
  }
  // Assignment move (i.e. steal data) operator
  Matrix &operator=(Matrix &&m) {
    _data = std::move(m._data);
    _rows = m._rows;
    _cols = m._cols;
    return *this;
  }

  // Access to the matrix coefficients is implemented via the call operator
  T &operator()(size_t row, size_t col) {
    return _data[row * _cols + col];
  }  // Subscript operators often come in pairs
  T operator()(size_t row, size_t col) const {
    return _data[row * _cols + col];
  };  // Subscript operators often come in pairs

  // We overload ^ for the matrix product operator
  friend Matrix operator^(const Matrix &lhs, const Matrix &rhs) {
    // the output matrix
    Matrix mat(lhs.rows(), rhs.cols());

    // makes sure the sizes match
    assert(lhs.cols() == rhs.rows());

    // naive matrix-matrix multiplication
    for (size_t row = 0; row < mat.rows(); row++)
      for (size_t col = 0; col < mat.cols(); col++)
        for (size_t mid = 0; mid < lhs.cols(); mid++)
          mat(row, col) += lhs(row, mid) * rhs(mid, col);

    return mat;
  }

  // method that fills the matrix in-place with normally distributeed
  // random values
  void normal_() {
    std::normal_distribution<> d{0.0, 1.0};

    for (size_t row = 0; row < _rows; row++)
      for (size_t col = 0; col < _cols; col++)
        _data[row * _cols + col] = d(rng);
  }
};

template <class T>
std::vector<Matrix<T>> make_random_matrices(size_t n_matrices, size_t rows,
                                            size_t cols) {
  /*
   * A method that creates many matrices of the same shape into a vector
   * and fills them with random values
   */
  std::vector<Matrix<T>> matrices;
  for (int n = 0; n < n_matrices; n++) {
    matrices.emplace_back(Matrix<T>(rows, cols));
    matrices.back().normal_();
  }
  return matrices;
}

int main(int argc, char **argv) {
  redpoule::ThreadPool thread_pool;

  auto lhs = make_random_matrices<float>(n_matrices, dim1, dim2);
  auto rhs = make_random_matrices<float>(n_matrices, dim2, dim3);

  auto time1 = std::chrono::system_clock::now();

  std::vector<Matrix<float>> results1;
  for (int n = 0; n < n_matrices; n++) results1.emplace_back(lhs[n] ^ rhs[n]);

  auto time2 = std::chrono::system_clock::now();

  std::vector<Matrix<float>> results2{n_matrices};

  thread_pool.parallel_for(0, n_matrices, [&](size_t bi, size_t ei) {
    for (size_t i = bi; i < ei; i++) {
      results2[i] = lhs[i] ^ rhs[i];
    }
  });

  auto time3 = std::chrono::system_clock::now();

  std::cout << "Time for direct computation: " << (time2 - time1).count()
            << std::endl;
  std::cout << "Time for parallel_for computation: " << (time3 - time2).count()
            << std::endl;

  double speed_up = (double)(time2 - time1).count() / (time3 - time2).count();
  std::cout << "Speed-up: " << speed_up << std::endl;
}
