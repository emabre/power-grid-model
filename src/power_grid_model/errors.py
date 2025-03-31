# SPDX-FileCopyrightText: Contributors to the Power Grid Model project <powergridmodel@lfenergy.org>
#
# SPDX-License-Identifier: MPL-2.0

"""
Error classes used by the power-grid-model library.
"""


import numpy as np


from power_grid_model._errors import (
    PowerGridError,
    PowerGridBatchError,
    InvalidArguments,
    MissingCaseForEnumError,
    ConflictVoltage,
    InvalidBranch,
    InvalidBranch3,
    InvalidTransformerClock,
    SparseMatrixError,
    NotObservableError,
    IterationDiverge,
    MaxIterationReached,
    IDNotFound,
    InvalidMeasuredObject,
    InvalidRegulatedObject,
    IDWrongType,
    InvalidCalculationMethod,
    AutomaticTapCalculationError,
    AutomaticTapInputError,
    InvalidShortCircuitPhaseOrType,
    PowerGridSerializationError,
    PowerGridDatasetError,
    PowerGridNotImplementedError,
    PowerGridUnreachableHitError,
)


class InvalidID(PowerGridError):
    """An ID is invalid."""


class ConflictID(InvalidID):
    """Conflicting IDs found."""