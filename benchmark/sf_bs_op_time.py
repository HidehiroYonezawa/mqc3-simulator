import secrets
import time
from math import log, pi

import strawberryfields as sf
from strawberryfields.ops import BSgate, Catstate, Squeezed


def bs_op_time_measurement(n_modes: int, n_cats: int) -> None:
    n_ops = n_modes * 10

    n_squeezed_states = n_modes - n_cats
    squeezing_level = 5
    r = squeezing_level * log(10) / 10

    prog = sf.Program(n_modes)
    with prog.context as q:
        for i in range(n_cats):
            Catstate(a=1.0, phi=secrets.SystemRandom().uniform(-pi, pi), p=0) | q[i]
        for i in range(n_cats, n_squeezed_states):
            Squeezed(r=r / 2, p=secrets.SystemRandom().uniform(-pi, pi) * 2) | q[i]
        for _ in range(n_ops):
            while True:
                mode0 = secrets.randbelow(n_modes)
                mode1 = secrets.randbelow(n_modes)
                if mode0 == mode1:
                    continue
                theta = secrets.SystemRandom().uniform(-pi, pi)
                phi = secrets.SystemRandom().uniform(-pi, pi)
                BSgate(theta=theta, phi=phi) | (q[mode0], q[mode1])
                break

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
        bs_op_time_measurement(n_modes=n_modes, n_cats=n_cats)
