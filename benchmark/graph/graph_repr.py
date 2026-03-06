from math import pi
from random import uniform

from mqc3.graph import GraphRepr
from mqc3.graph.ops import Initialization, Measurement, PhaseRotation


def create_graph_repr(
    n_local_macronodes: int,
    n_steps: int,
) -> GraphRepr:
    graph_repr = GraphRepr(n_local_macronodes, n_steps)

    for i in range(n_local_macronodes):
        graph_repr.place_operation(
            Initialization(macronode=(i, 0), theta=0.0, initialized_modes=(-1, i), readout=False),
        )
    for step in range(1, n_steps - 1):
        for i in range(n_local_macronodes):
            graph_repr.place_operation(
                PhaseRotation(
                    macronode=(i, step),
                    phi=uniform(-pi, pi),
                    swap=False,
                    displacement_k_minus_1=(uniform(-10.0, 10.0), uniform(-10.0, 10.0)),
                    displacement_k_minus_n=(uniform(-10.0, 10.0), uniform(-10.0, 10.0)),
                ),
            )
    for i in range(n_local_macronodes):
        graph_repr.place_operation(
            Measurement(macronode=(i, n_steps - 1), theta=pi, readout=True),
        )

    return graph_repr
