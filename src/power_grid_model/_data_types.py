from typing import TypeAlias, TypeVar, TypedDict

import numpy as np


SingleArray: TypeAlias = np.ndarray
"""
A single array is a one-dimensional structured numpy array containing a list of components of the same type.

- Examples:

    - structure: <1d-array>
    - concrete: array([(0, 10500.0), (0, 10500.0)], dtype=power_grid_meta_data["input"]["node"].dtype)
"""

AttributeType: TypeAlias = str
"""
An attribute type is a string reprenting the attribute type of a specific component.

- Examples:

    - "id"
    - "u_rated"
"""


SingleColumnarData = dict[AttributeType, SingleColumn]
"""
Single columnar data is a dictionary where the keys are the attribute types of the same component
and the values are :class:`SingleColumn`.

- Example: {"id": :class:`AttributeType`, "u_rated": :class:`SingleColumn`}
"""

_SingleComponentData = TypeVar("_SingleComponentData", SingleArray, SingleColumnarData)  # deduction helper
SingleComponentData = SingleArray | SingleColumnarData
"""
Single component data can be :class:`SingleArray` or :class:`SingleColumnarData`.
"""


SingleDataset = dict[ComponentTypeVar, _SingleComponentData]
"""
A single dataset is a dictionary where the keys are the component types and the values are
:class:`ComponentData`

- Example: {"node": :class:`SingleArray`, "line": :class:`SingleColumnarData`}
"""

ColumnarData = SingleColumnarData | BatchColumnarData
"""
Columnar data can be :class:`SingleColumnarData` or :class:`BatchColumnarData`.
"""



_ComponentData = TypeVar("_ComponentData", SingleComponentData, BatchComponentData)  # deduction helper
ComponentData = DataArray | ColumnarData
"""
Component data can be :class:`DataArray` or :class:`ColumnarData`.
"""

Dataset = dict[ComponentTypeVar, _ComponentData]
"""
A general data set can be a :class:`SingleDataset` or a :class:`BatchDataset`.

- Examples:

    - single: {"node": :class:`SingleArray`, "line": :class:`SingleColumnarData`}

    - batch: {"node": :class:`DenseBatchArray`, "line": :class:`SparseBatchArray`,
             "link": :class:`DenseBatchColumnarData`, "transformer": :class:`SparseBatchColumnarData`}

"""


DenseBatchData = DenseBatchArray | DenseBatchColumnarData
"""
Dense batch data can be a :class:`DenseBatchArray` or a :class:`DenseBatchColumnarData`.
"""


IndexPointer: TypeAlias = np.ndarray
"""
An index pointer is a one-dimensional numpy int64 array containing n+1 elements where n is the amount
of scenarios, representing the start and end indices for each batch scenario as follows:

    - The elements are the indices in the data that point to the first element of that scenario.
    - The last element is one after the data index of the last element of the last scenario.
    - The first element and last element will therefore be 0 and the size of the data, respectively.
"""

class SparseBatchArray(TypedDict):
    """
    A sparse batch array is a dictionary containing the keys `indptr` and `data`.

    - data: a :class:`SingleArray`. The exact dtype depends on the type of component.
    - indptr: an :class:`IndexPointer` representing the start and end indices for each batch scenario.

    - Examples:

        - structure: {"indptr": :class:`IndexPointer`, "data": :class:`SingleArray`}
        - concrete example: {"indptr": [0, 2, 2, 3], "data": [(0, 1, 1), (1, 1, 1), (0, 0, 0)]}

            - the scenario 0 sets the statuses of components with ids 0 and 1 to 1
              (and keeps defaults for other components)
            - scenario 1 keeps the default values for all components
            - scenario 2 sets the statuses of component with id 0 to 0 (and keeps defaults for other components)
    """

    indptr: IndexPointer
    data: SingleArray


SparseBatchData = SparseBatchArray | SparseBatchColumnarData
"""
Sparse batch data can be a :class:`SparseBatchArray` or a :class:`SparseBatchColumnarData`.
"""


DenseBatchArray: TypeAlias = np.ndarray
"""
A dense batch array is a two-dimensional structured numpy array containing a list of components of 
the same type for each scenario. Otherwise similar to :class:`SingleArray`.
"""



DenseBatchColumnarData = dict[AttributeType, BatchColumn]
"""
Batch columnar data is a dictionary where the keys are the attribute types of the same component
and the values are :class:`BatchColumn`.

- Example: {"id": :class:`AttributeType`, "from_status": :class:`BatchColumn`}
"""


class SparseBatchColumnarData(TypedDict):
    """
    Sparse batch columnar data is a dictionary containing the keys `indptr` and `data`.

    - data: a :class:`SingleColumnarData`. The exact supported attribute columns depend on the component type.
    - indptr: an :class:`IndexPointer` representing the start and end indices for each batch scenario.

    - Examples:

        - structure: {"indptr": :class:`IndexPointer`, "data": :class:`SingleColumnarData`}
        - concrete example: {"indptr": [0, 2, 2, 3], "data": {"id": [0, 1, 0], "status": [1, 1, 0]}}

            - the scenario 0 sets the status of components with ids 0 and 1 to 1
              (and keeps defaults for other components)
            - scenario 1 keeps the default values for all components
            - scenario 2 sets the status of component with id 0 to 0 (and keeps defaults for other components)
    """

    indptr: IndexPointer
    data: SingleColumnarData
