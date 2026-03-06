import random
import secrets
import time
from math import log, pi

import strawberryfields as sf
from strawberryfields.ops import Catstate, Rgate, Squeezed


def rotation_op_time_measurement(n_modes: int, n_cats: int) -> None:
    n_repeat = 10
    mode_idxs = [mode for mode in range(n_modes) for _ in range(n_repeat)]
    random.shuffle(mode_idxs)

    n_squeezed_states = n_modes - n_cats
    squeezing_level = 5
    r = squeezing_level * log(10) / 10

    prog = sf.Program(n_modes)
    with prog.context as q:
        for i in range(n_cats):
            Catstate(a=1.0, phi=secrets.SystemRandom().uniform(-pi, pi), p=0) | q[i]
        for i in range(n_cats, n_squeezed_states):
            Squeezed(r=r / 2, p=secrets.SystemRandom().uniform(-pi, pi) * 2) | q[i]
        for mode in mode_idxs:
            Rgate(theta=secrets.SystemRandom().uniform(-pi, pi)) | q[mode]

    eng = sf.Engine("bosonic")
    start = time.time()
    result = eng.run(prog, shots=1)
    end = time.time()
    execution_time_ms = (end - start) * 1000
    print(f"{n_modes},{n_cats},{execution_time_ms}")

    state = result.state
    assert state is not None
    assert state.num_modes == n_modes
    assert state.num_weights == 4**n_cats


v_n_modes = [10, 100, 1000]
v_n_cats = [0, 2, 5]
print("n_modes,n_cats,time[ms]")
for n_modes in v_n_modes:
    for n_cats in v_n_cats:
        rotation_op_time_measurement(n_modes=n_modes, n_cats=n_cats)
