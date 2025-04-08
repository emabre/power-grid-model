# SPDX-FileCopyrightText: Contributors to the Power Grid Model project <powergridmodel@lfenergy.org>
#
# SPDX-License-Identifier: MPL-2.0

"""
Data types involving PGM datasets.

Many data types are used throughout the power grid model project. In an attempt to clarify type hints, some types
have been defined and explained in this file.
"""

from typing import TypeAlias, TypeVar

import numpy as np

from power_grid_model._core.dataset_definitions import ComponentTypeVar

from power_grid_model._core.data_types import (
    SingleArray,
    AttributeType,
    SingleColumn,
    DenseBatchArray,
    SingleColumnarData,
    SingleDataset,
    BatchColumn,
    DenseBatchColumnarData,
    IndexPointer,
    SparseBatchColumnarData,
    SparseBatchArray,
    SparseBatchData,
    BatchColumnarData,
    ColumnarData,
    BatchArray,
    BatchComponentData,
    DataArray,
    ComponentData,
    Dataset,
    DenseBatchData,
)

SparseDataComponentType: TypeAlias = str
"""
A string representing the component type of sparse data structures.

Must be either "data" or "indptr".
"""




_BatchComponentData = TypeVar("_BatchComponentData", BatchArray, BatchColumnarData)  # deduction helper


BatchDataset = dict[ComponentTypeVar, _BatchComponentData]
"""
A batch dataset is a dictionary where the keys are the component types and the values are :class:`BatchComponentData`

- Example: {"node": :class:`DenseBatchArray`, "line": :class:`SparseBatchArray`,
            "link": :class:`DenseBatchColumnarData`, "transformer": :class:`SparseBatchColumnarData`}
"""

BatchList = list[SingleDataset]
"""
A batch list is an alternative representation of a batch. It is a list of single datasets, where each single dataset
is actually a batch. The batch list is intended as an intermediate data type, during conversions.

- Example: [:class:`SingleDataset`, {"node": :class:`SingleDataset`}]
"""

NominalValue = int
"""
Nominal values can be IDs, booleans, enums, tap pos.

- Example: 123
"""

RealValue = float
"""
Symmetrical values can be anything like cable properties, symmetric loads, etc.

- Example: 10500.0
"""

AsymValue = tuple[RealValue, RealValue, RealValue]
"""
Asymmetrical values are three-phase values like p or u_measured.

- Example: (10400.0, 10500.0, 10600.0)
"""

AttributeValue = RealValue | NominalValue | AsymValue
"""
When representing a grid as a native python structure, each attribute (u_rated etc) is either a nominal value,
a real value, or a tuple of three real values.

- Examples:

    - real: 10500.0
    - nominal: 123
    - asym: (10400.0, 10500.0, 10600.0)
"""

Component = dict[AttributeType, AttributeValue | str]
"""
A component, when represented in native python format, is a dictionary, where the keys are the attributes and the values
are the corresponding values. It is allowed to add extra fields, containing either an AttributeValue or a string.

- Example: {"id": 1, "u_rated": 10500.0, "original_id": "Busbar #1"}
"""

ComponentList = list[Component]
"""
A component list is a list containing components. In essence it stores the same information as a np.ndarray,
but in a native python format, without using numpy.

- Example: [{"id": 1, "u_rated": 10500.0}, {"id": 2, "u_rated": 10500.0}]
"""

SinglePythonDataset = dict[ComponentTypeVar, ComponentList]
"""
A single dataset in native python representation is a dictionary, where the keys are the component names and the
values are a list of all the instances of such a component. In essence it stores the same information as a
SingleDataset, but in a native python format, without using numpy.

- Example:

  {
    "node": [{"id": 1, "u_rated": 10500.0}, {"id": 2, "u_rated": 10500.0}], 
    "line": [{"id": 3, "from_node": 1, "to_node": 2, ...}],
  }
"""

BatchPythonDataset = list[SinglePythonDataset]
"""
A batch dataset in native python representation is a list of dictionaries, where the keys are the component names and
the values are a list of all the instances of such a component. In essence it stores the same information as a
BatchDataset, but in a native python format, without using numpy. Actually it looks more like the BatchList.

- Example:

  [{"line": [{"id": 3, "from_status": 0, "to_status": 0, ...}],},
   {"line": [{"id": 3, "from_status": 1, "to_status": 1, ...}],}]
"""

PythonDataset = SinglePythonDataset | BatchPythonDataset
"""
A general python data set can be a single or a batch python dataset.

- Examples:

  - single:

    {
      "node": [{"id": 1, "u_rated": 10500.0}, {"id": 2, "u_rated": 10500.0}],
      "line": [{"id": 3, "from_node": 1, "to_node": 2, ...}],
    }

  - batch:

    [{"line": [{"id": 3, "from_status": 0, "to_status": 0, ...}],},
     {"line": [{"id": 3, "from_status": 1, "to_status": 1, ...}],}]
"""
