#pragma once

#include "core/Types.hpp"
#include <complex>
#include <stdexcept>
#include <vector>

namespace GLStation::Util {

template <typename T> class SparseMatrix {
public:
  SparseMatrix(size_t rows, size_t cols) : m_rows(rows), m_cols(cols) {
    m_rowPtr.assign(rows + 1, 0);
  }

  void setDirect(const std::vector<T> &values,
                 const std::vector<size_t> &colIndex,
                 const std::vector<size_t> &rowPtr) {
    m_values = values;
    m_colIndex = colIndex;
    m_rowPtr = rowPtr;
  }

  size_t getRows() const { return m_rows; }
  size_t getCols() const { return m_cols; }
  const std::vector<T> &getValues() const { return m_values; }
  const std::vector<size_t> &getColIndex() const { return m_colIndex; }
  const std::vector<size_t> &getRowPtr() const { return m_rowPtr; }

  T getAt(size_t row, size_t col) const {
    for (size_t j = m_rowPtr[row]; j < m_rowPtr[row + 1]; ++j) {
      if (m_colIndex[j] == col)
        return m_values[j];
    }
    return T(0);
  }

  std::vector<T> multiply(const std::vector<T> &x) const {
    if (x.size() != m_cols)
      throw std::runtime_error("Dimension mismatch in MXX");
    std::vector<T> result(m_rows, T(0));
    for (size_t i = 0; i < m_rows; ++i) {
      for (size_t j = m_rowPtr[i]; j < m_rowPtr[i + 1]; ++j) {
        result[i] += m_values[j] * x[m_colIndex[j]];
      }
    }
    return result;
  }

private:
  size_t m_rows;
  size_t m_cols;
  std::vector<T> m_values;
  std::vector<size_t> m_colIndex;
  std::vector<size_t> m_rowPtr;
};

} // namespace GLStation::Util
