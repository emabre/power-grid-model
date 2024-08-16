// SPDX-FileCopyrightText: Contributors to the Power Grid Model project <powergridmodel@lfenergy.org>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once
#ifndef POWER_GRID_MODEL_CPP_DATASET_HPP
#define POWER_GRID_MODEL_CPP_DATASET_HPP

#include "power_grid_model_c/dataset.h"

#include "basics.hpp"
#include "handle.hpp"

namespace power_grid_model_cpp {
class Dataset {
  public:
    class Info {
      public:
        Info(Dataset const& dataset) : dataset_{dataset} {}

        static std::string const name(Handle const& handle, DatasetInfo const* info) {
            auto const name = std::string(PGM_dataset_info_name(handle.get(), info));
            handle.check_error();
            return name;
        }
        std::string const name(DatasetInfo const* info) const { return name(dataset_.handle_, info); }

        static Idx is_batch(Handle const& handle, DatasetInfo const* info) {
            auto const is_batch = PGM_dataset_info_is_batch(handle.get(), info);
            handle.check_error();
            return is_batch;
        }
        Idx is_batch(DatasetInfo const* info) const { return is_batch(dataset_.handle_, info); }

        static Idx batch_size(Handle const& handle, DatasetInfo const* info) {
            auto const batch_size = PGM_dataset_info_batch_size(handle.get(), info);
            handle.check_error();
            return batch_size;
        }
        Idx batch_size(DatasetInfo const* info) const { return batch_size(dataset_.handle_, info); }

        static Idx n_components(Handle const& handle, DatasetInfo const* info) {
            auto const n_components = PGM_dataset_info_n_components(handle.get(), info);
            handle.check_error();
            return n_components;
        }
        Idx n_components(DatasetInfo const* info) const { return n_components(dataset_.handle_, info); }

      private:
        friend class Dataset;

        Dataset const& dataset_;
    };

    class ComponentInfo {
      public:
        ComponentInfo(Dataset const& dataset) : dataset_{dataset} {}

        static std::string const name(Handle const& handle, DatasetInfo const* info, Idx component_idx) {
            auto const component_name = std::string(PGM_dataset_info_component_name(handle.get(), info, component_idx));
            handle.check_error();
            return component_name;
        }
        std::string const name(DatasetInfo const* info, Idx component_idx) const {
            return name(dataset_.handle_, info, component_idx);
        }

        static Idx elements_per_scenario(Handle const& handle, DatasetInfo const* info, Idx component_idx) {
            auto const elements_per_scenario =
                PGM_dataset_info_elements_per_scenario(handle.get(), info, component_idx);
            handle.check_error();
            return elements_per_scenario;
        }
        Idx elements_per_scenario(DatasetInfo const* info, Idx component_idx) const {
            return elements_per_scenario(dataset_.handle_, info, component_idx);
        }

        static Idx total_elements(Handle const& handle, DatasetInfo const* info, Idx component_idx) {
            auto const total_elements = PGM_dataset_info_total_elements(handle.get(), info, component_idx);
            handle.check_error();
            return total_elements;
        }
        Idx total_elements(DatasetInfo const* info, Idx component_idx) const {
            total_elements(dataset_.handle_, info, component_idx);
        }

      private:
        friend class Dataset;

        Dataset const& dataset_;
    };

    Info info;
    ComponentInfo component_info;

    Handle* get_handle() { return &handle_; }

  protected:
    Dataset() : info{*this}, component_info{*this} {}
    Handle handle_{};
};

class DatasetConst : public Dataset {
  public:
    DatasetConst(std::string const& dataset, Idx is_batch, Idx batch_size)
        : Dataset(), dataset_{PGM_create_dataset_const(handle_.get(), dataset.c_str(), is_batch, batch_size)} {}
    DatasetConst(WritableDataset const* writable_dataset)
        : Dataset(), dataset_{PGM_create_dataset_const_from_writable(handle_.get(), writable_dataset)} {}
    DatasetConst(MutableDataset const* mutable_dataset)
        : Dataset(), dataset_{PGM_create_dataset_const_from_mutable(handle_.get(), mutable_dataset)} {}

    

    static void add_buffer(DatasetConst& dataset, std::string const& component, Idx elements_per_scenario,
                           Idx total_elements, Idx const* indptr, RawDataConstPtr data) {
        PGM_dataset_const_add_buffer(dataset.handle_.get(), dataset.get(), component.c_str(), elements_per_scenario,
                                     total_elements, indptr, data);
        dataset.handle_.check_error();
    }
    void add_buffer(std::string const& component, Idx elements_per_scenario, Idx total_elements, Idx const* indptr,
                    RawDataConstPtr data) {
        return add_buffer(*this, component, elements_per_scenario, total_elements, indptr, data);
    }

    static DatasetInfo const* get_info(DatasetConst const& dataset) {
        PGM_dataset_const_get_info(dataset.handle_.get(), dataset.get());
        dataset.handle_.check_error();
    }
    DatasetInfo const* get_info() const { return get_info(*this); }

  private:
    detail::UniquePtr<ConstDataset, PGM_destroy_dataset_const> dataset_;

    ConstDataset* get() const { return dataset_.get(); }
};

class DatasetWritable : public Dataset {
  public:
    DatasetWritable() : Dataset() {}

    static DatasetInfo const* get_info(Handle const& handle, WritableDataset const* dataset) {
        return PGM_dataset_writable_get_info(handle.get(), dataset);
        handle.check_error();
    }
    DatasetInfo const* get_info(WritableDataset const* dataset) const { return get_info(handle_, dataset); }

    static void set_buffer(Handle const& handle, WritableDataset* dataset, std::string const& component, Idx* indptr,
                           RawDataPtr data) {
        PGM_dataset_writable_set_buffer(handle.get(), dataset, component.c_str(), indptr, data);
        handle.check_error();
    }
    void set_buffer(WritableDataset* dataset, std::string const& component, Idx* indptr, RawDataPtr data) {
        return set_buffer(handle_, dataset, component.c_str(), indptr, data);
    }
};

class DatasetMutable : public Dataset {
    DatasetMutable(std::string const& dataset, Idx is_batch, Idx batch_size)
        : Dataset(), dataset_{PGM_create_dataset_mutable(handle_.get(), dataset.c_str(), is_batch, batch_size)} {}

    MutableDataset* get() const { return dataset_.get(); }

    static void add_buffer(DatasetMutable& dataset, std::string const& component, Idx elements_per_scenario,
                           Idx total_elements, Idx const* indptr, RawDataPtr data) {
        PGM_dataset_mutable_add_buffer(dataset.handle_.get(), dataset.get(), component.c_str(), elements_per_scenario,
                                       total_elements, indptr, data);
        dataset.handle_.check_error();
    }
    void add_buffer(std::string const& component, Idx elements_per_scenario, Idx total_elements, Idx const* indptr,
                    RawDataPtr data) {
        return add_buffer(*this, component.c_str(), elements_per_scenario, total_elements, indptr, data);
    }

    static DatasetInfo const* get_info(DatasetMutable const& dataset) {
        PGM_dataset_mutable_get_info(dataset.handle_.get(), dataset.get());
        dataset.handle_.check_error();
    }
    DatasetInfo const* get_info() const { return get_info(*this); }

  private:
    detail::UniquePtr<MutableDataset, PGM_destroy_dataset_mutable> dataset_;
};
} // namespace power_grid_model_cpp

#endif // POWER_GRID_MODEL_CPP_DATASET_HPP
