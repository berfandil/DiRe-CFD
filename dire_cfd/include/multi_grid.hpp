///
/// @copyrightblock
///
///   NPOSL-3.0 License
///
///   Copyright(c) 2021 Mátyás Léránt-Nyeste
///
///   Licensed under the Non-Profit Open Software License, version 3.0 (the "License"); you may not use this file except
///   in compliance with the License. You may obtain a copy of the License at:
///
///   https://opensource.org/licenses/NPOSL-3.0
///
///   Unless required by applicable law or agreed to in writing, software distributed under the License is distributed
///   on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for
///   the specific language governing permissionsand limitations under the License.
///
/// @endcopyrightblock
///

#pragma once

#include <array>
#include <vector>
#include <numeric>
#include <stdexcept>

namespace dire {

/// @brief A grid of the given dimensionality, that can hold multiple elements in each cell.
template <size_t dim, class Data>
class MultiGrid
{
private:

    static_assert(dim > 0, "The dimensionality must be greater, than zero!");
    static_assert(std::is_default_constructible<Data>::value, "Data has to be default constructible.");

public:

    using GridSize = std::array<size_t, dim>;
    using CellId = std::array<size_t, dim>;

    struct DataBounds
    {
        typename std::vector<Data>::const_iterator begin;
        typename std::vector<Data>::const_iterator end;
    };

    /// @brief Constructor.
    /// @param grid_size The number of cells there are in the grid along each dimension.
    /// @param buff_size The maximum number of data that can be stored in the buffer. If more data is supplied, than
    ///                  this amount, the underlying "std::vector" containing the datas is resized, caused by it's
    ///                  "push_back" function.
    /// @throws std::runtime_error If any of the grid sizes is zero.
    MultiGrid(GridSize grid_size, size_t buff_size = 0);

    /// @brief Adds a data to the given cell of the grid. Makes the grid uncompressed.
    /// @param cell_id The id of the cell.
    /// @param data The data.
    /// @throws std::out_of_range If an invalid cell id is provided.
    void add(const CellId& cell_id, Data&& data);

    /// @brief Clears all buffered data from the grid. Makes the grid uncompressed.
    void clear();

    /// @brief Converts the grid into a compressed format.
    void compress();

    /// @brief Enumerates all data in the given cell. The grid has to be compressed.
    /// @param cell_id The id of the cell.
    /// @return The enumerated data represented by it's begin and end iterators.
    /// @throws std::runtime_error If the grid is not compressed.
    /// @throws std::out_of_range If an invalid cell id is provided.
    DataBounds enumerateData(const CellId& cell_id) const;

private:

    /// @brief Linearizes a cell id. Used for computing the storage id corresponding to the cell.
    /// @param cell_id The cell id.
    /// @return The storage id.
    size_t linearize(const CellId& cell_id) const;

    /// @brief The stored data in uncompressed form.
    struct RawData
    {
        std::vector<Data> data;                        ///< The data buffered before compression
        std::vector<CellId> cell_ids; ///< The cell ids of the data buffered before compression
    };

    /// @brief The stored data in compressed form.
    struct CompressedData
    {
        std::vector<Data> data;                         ///< The stored data in a compressed format
        std::vector<size_t> num_data_per_cell;          ///< The number of datas stored in each cell of the grid
        std::vector<size_t> first_data_id_per_cell;     ///< The id of the first data stored in each cell
        std::vector<size_t> next_data_id_per_cell_buff; ///< Buffer used for initializing the compressed data
    };

    GridSize grid_size_;             ///< The number of cells there are in the grid along each dimension
    size_t num_cells_;               ///< The gross number of cells in the grid
    bool compressed_;                ///< Whether the stored data is compressed
    RawData raw_data_;               ///< The stored data in uncompressed form
    CompressedData compressed_data_; ///< The stored data in compressed form
};

//======================================================================================================================

template <size_t dim, class Data>
MultiGrid<dim, Data>::MultiGrid(GridSize grid_size, size_t buff_size)
    : grid_size_(std::move(grid_size))
    , num_cells_(std::accumulate(grid_size_.begin(), grid_size_.end(), 1, std::multiplies<size_t>()))
    , compressed_(false)
{
    for (size_t i = 0; i < dim; ++i)
    {
        if (grid_size_[i] == 0)
        {
            throw std::runtime_error("All grid sizes have to be greater, than zero!");
        }
    }

    raw_data_.data.reserve(buff_size);
    raw_data_.cell_ids.reserve(buff_size);

    compressed_data_.data.reserve(buff_size);
    compressed_data_.num_data_per_cell.resize(num_cells_, 0);
    compressed_data_.first_data_id_per_cell.resize(num_cells_, 0);
    compressed_data_.next_data_id_per_cell_buff.resize(num_cells_, 0);
}

template <size_t dim, class Data>
void MultiGrid<dim, Data>::add(const CellId& cell_id, Data&& data)
{
    for (size_t i = 0; i < dim; ++i)
    {
        if (cell_id[i] >= grid_size_[i])
        {
            throw std::out_of_range("MultiGrid::add(): Invalid cell id!");
        }
    }

    if (compressed_)
    {
        compressed_ = false;
    }

    raw_data_.data.push_back(data);
    raw_data_.cell_ids.push_back(cell_id);
}

template <size_t dim, class Data>
void MultiGrid<dim, Data>::clear()
{
    if (compressed_)
    {
        compressed_ = false;
    }

    raw_data_.data.clear();
    raw_data_.cell_ids.clear();
}

template <size_t dim, class Data>
void MultiGrid<dim, Data>::compress()
{
    if (compressed_)
    {
        return;
    }

    // Compute how much data is stored in each cell
    std::fill(compressed_data_.num_data_per_cell.begin(), compressed_data_.num_data_per_cell.end(), 0);
    for (const auto& cell_id : raw_data_.cell_ids)
    {
        const auto storage_id = linearize(cell_id);
        ++compressed_data_.num_data_per_cell[storage_id];
    }

    // Compute the starting ids of data in the compressed fromat for each cell
    std::fill(compressed_data_.first_data_id_per_cell.begin(), compressed_data_.first_data_id_per_cell.end(), 0);
    size_t first_data_id_buff = 0;
    for (size_t i = 0; i < num_cells_; ++i)
    {
        compressed_data_.first_data_id_per_cell[i] = first_data_id_buff;
        first_data_id_buff += compressed_data_.num_data_per_cell[i];
    }

    // Write the compressed data
    compressed_data_.data.resize(raw_data_.data.size());
    compressed_data_.next_data_id_per_cell_buff = compressed_data_.first_data_id_per_cell;
    const auto num_raw_data = raw_data_.data.size();
    for (size_t i = 0; i < num_raw_data; ++i)
    {
        const auto storage_id = linearize(raw_data_.cell_ids[i]);
        const auto next_data_id = compressed_data_.next_data_id_per_cell_buff[storage_id]++;
        compressed_data_.data[next_data_id] = raw_data_.data[i];
    }

    compressed_ = true;
}

template <size_t dim, class Data>
typename MultiGrid<dim, Data>::DataBounds MultiGrid<dim, Data>::enumerateData(const CellId& cell_id) const
{
    if (!compressed_)
    {
        throw std::runtime_error("MultiGrid::enumerateData(): The grid has to be compressed!");
    }

    for (size_t i = 0; i < dim; ++i)
    {
        if (cell_id[i] >= grid_size_[i])
        {
            throw std::out_of_range("MultiGrid::enumerateData(): Invalid cell id!");
        }
    }

    const auto storage_id = linearize(cell_id);
    const auto begin_it = compressed_data_.data.begin() + compressed_data_.first_data_id_per_cell[storage_id];
    const auto end_it = begin_it + compressed_data_.num_data_per_cell[storage_id];
    return { begin_it, end_it };
}

template <size_t dim, class Data>
size_t MultiGrid<dim, Data>::linearize(const CellId& cell_id) const
{
    size_t storage_id = 0;
    size_t mult = 1;
    for (size_t i = 0; i < dim; ++i)
    {
        storage_id += cell_id[i] * mult;
        mult *= grid_size_[i];
    }
    return storage_id;
}

} // end namespace dire
