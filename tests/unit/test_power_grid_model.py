# SPDX-FileCopyrightText: 2022 Contributors to the Power Grid Model project <dynamic.grid.calculation@alliander.com>
#
# SPDX-License-Identifier: MPL-2.0

from copy import copy
from pathlib import Path

import numpy as np
import pytest

from power_grid_model import PowerGridModel, initialize_array
from power_grid_model.errors import PowerGridBatchError, PowerGridError

from .utils import compare_result, import_case_data

"""
Testing network

source_1(1.0 p.u., 100.0 V) --internal_impedance(j10.0 ohm, sk=1000.0 VA, rx_ratio=0.0)--
-- node_0 (100.0 V) --load_2(const_i, -j5.0A, 0.0 W, 500.0 var)

u0 = 100.0 V - (j10.0 ohm * -j5.0 A) = 50.0 V

update_0:
    u_ref = 0.5 p.u. (50.0 V)
    q_specified = 100 var (-j1.0A)
u0 = 50.0 V - (j10.0 ohm * -j1.0 A) = 40.0 V

update_1:
    q_specified = 300 var (-j3.0A)
u0 = 100.0 V - (j10.0 ohm * -j3.0 A) = 70.0 V
"""


@pytest.fixture
def case_data():
    case_dir = Path(__file__).parent.parent / "data" / "unit_test"
    return import_case_data(case_dir, sym=True)


@pytest.fixture
def model(case_data):
    return PowerGridModel(input_data=case_data["input"])


def test_simple_power_flow(model: PowerGridModel, case_data):
    result = model.calculate_power_flow()
    compare_result(result, case_data["output"], rtol=0.0, atol=1e-8)


def test_simple_update(model: PowerGridModel, case_data):
    update_batch = case_data["update_batch"]
    source_indptr = update_batch["source"]["indptr"]
    source_update = update_batch["source"]["data"]
    update_data = {
        "source": source_update[source_indptr[0] : source_indptr[1]],
        "sym_load": update_batch["sym_load"][0, :],
    }
    model.update(update_data=update_data)
    expected_result = {"node": case_data["output_batch"]["node"][0, :]}
    result = model.calculate_power_flow()
    compare_result(result, expected_result, rtol=0.0, atol=1e-8)


def test_update_error(model: PowerGridModel):
    load_update = initialize_array("update", "sym_load", 1)
    load_update["id"] = 5
    update_data = {"sym_load": load_update}
    with pytest.raises(PowerGridError, match="The id cannot be found:"):
        model.update(update_data=update_data)


def test_copy_model(model: PowerGridModel, case_data):
    model_2 = copy(model)
    result = model_2.calculate_power_flow()
    compare_result(result, case_data["output"], rtol=0.0, atol=1e-8)


def test_get_indexer(model: PowerGridModel):
    ids = np.array([2, 2])
    expected_indexer = np.array([0, 0])
    indexer = model.get_indexer("sym_load", ids)
    assert np.allclose(expected_indexer, indexer)


def test_batch_power_flow(model: PowerGridModel, case_data):
    result = model.calculate_power_flow(update_data=case_data["update_batch"])
    compare_result(result, case_data["output_batch"], rtol=0.0, atol=1e-8)


def test_construction_error(case_data):
    case_data["input"]["sym_load"]["id"] = 0
    with pytest.raises(PowerGridError, match="Conflicting id detected:"):
        PowerGridModel(case_data["input"])


def test_single_calculation_error(model: PowerGridModel):
    with pytest.raises(PowerGridError, match="Iteration failed to converge after"):
        model.calculate_power_flow(max_iterations=1, error_tolerance=1e-100)
    with pytest.raises(PowerGridError, match="The calculation method is invalid for this calculation!"):
        model.calculate_state_estimation(calculation_method="iterative_current")


def test_batch_calculation_error(model: PowerGridModel, case_data):
    # wrong id
    case_data["update_batch"]["sym_load"]["id"][1, 0] = 5
    # with error
    with pytest.raises(PowerGridBatchError) as e:
        model.calculate_power_flow(update_data=case_data["update_batch"])
    error = e.value
    np.allclose(error.failed_scenarios, [1])
    np.allclose(error.succeeded_scenarios, [0])
    assert "The id cannot be found:" in error.error_messages[0]


def test_batch_calculation_error_continue(model: PowerGridModel, case_data):
    # wrong id
    case_data["update_batch"]["sym_load"]["id"][1, 0] = 5
    result = model.calculate_power_flow(update_data=case_data["update_batch"], continue_on_batch_error=True)
    # assert error
    error = model.batch_error
    assert error is not None
    np.allclose(error.failed_scenarios, [1])
    np.allclose(error.succeeded_scenarios, [0])
    assert "The id cannot be found:" in error.error_messages[0]
    # assert value result for scenario 0
    result = {"node": result["node"][error.succeeded_scenarios, :]}
    expected_result = {"node": case_data["output_batch"]["node"][error.succeeded_scenarios, :]}
    compare_result(result, expected_result, rtol=0.0, atol=1e-8)
