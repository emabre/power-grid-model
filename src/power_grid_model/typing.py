# SPDX-FileCopyrightText: Contributors to the Power Grid Model project <powergridmodel@lfenergy.org>
#
# SPDX-License-Identifier: MPL-2.0

"""
Type hints for the power-grid-model library.

This includes all miscellaneous type hints not under dataset or categories.
"""

from power_grid_model._core.dataset_definitions import (  # pylint: disable=unused-import
    ComponentType,
    ComponentTypeVar,
    DatasetType,
    DatasetTypeVar,
)
from power_grid_model._core.power_grid_meta import (  # pylint: disable=unused-import
    ComponentMetaData,
    DatasetMetaData,
    PowerGridMetaData,
)

# [Ema] Mention that you deleted this in the PR It's documented only as part of enum, and was useful before splitting away _typing.
# from power_grid_model.enum import ComponentAttributeFilterOptions

from power_grid_model._core.typing import ComponentAttributeMapping  # pylint: disable=unused-import