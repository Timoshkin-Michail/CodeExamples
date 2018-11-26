import numpy as np
from numpy import linalg as LA
import math
import matplotlib.pyplot as plt
import matplotlib.animation as anime


def fun(t, x):
    return 0.0


def fi_1(t):
    return 0.0


def fi_2(t):
    return 0.0


def xi_1(t):
    return 1.0 + t


def xi_2(t):
    return 1.0 + t


def u_0(x):
    return 3 * math.sin(2 * x) - math.sin(3 * x)


# u(x, t0)
def u(x):
    return 3 * math.exp(-4 * a ** 2 * t0) * math.sin(2 * x) - math.exp(
        -9 * a ** 2 * t0
    ) * math.sin(3 * x)


a = 1.0
# from equation


def init():
    line.set_xdata(x_axis)
    line.set_ydata(np.zeros_like(x_axis))
    return line,


def run(data):
    # update the data
    line.set_ydata(data)
    ax.relim()
    ax.autoscale_view()
    return line,


def explicit_scheme():
    previous_time_line = zero_time_line
    #print(zero_time_line.tolist())
    current_time_line = np.zeros(M + 1)
    for n in range(1, N+1):
        for m in range(1, M):
            current_time_line[m] = (
                previous_time_line[m]
                + tau
                * a ** 2
                / h ** 2
                * (
                    previous_time_line[m + 1]
                    - 2 * previous_time_line[m]
                    + previous_time_line[m - 1]
                )
                + tau * fun(tau * n, h * m)
            )
        if left_bound_cond == 1:
            current_time_line[0] = fi_1(n * tau)
        else:
            current_time_line[0] = (
                4 * current_time_line[1]
                - 2 * h * xi_1(n * tau)
                - current_time_line[2]
            ) / 3

        if right_bound_cond == 1:
            current_time_line[M] = fi_2(n * tau)
        else:
            current_time_line[M] = (
                4 * current_time_line[M - 1]
                + 2 * h * xi_2(n * tau)
                - current_time_line[M - 2]
            ) / 3
        np.put(
            previous_time_line, [i for i in range(M + 1)], current_time_line
        )
        #print(n, ')    ', current_time_line.tolist())
        data_for_plot.append(current_time_line)
    if mode == 1:
        norm(current_time_line)


def implicit_scheme():
    previous_time_line = zero_time_line
    current_time_line = np.zeros(M + 1)
    A = B = a ** 2 * tau / h ** 2
    C = 1 + 2 * A
    alpha = np.zeros(M + 1)
    beta = np.zeros(M + 1)
    for n in range(1, N + 1):
        if left_bound_cond == 1:
            alpha[1] = 0
            beta[1] = fi_2(n * tau)
        else:
            alpha[1] = 1
            beta[1] = -h * xi_1(n * tau)
        for i in range(1, M):
            F = fun((n+1) * tau, i * h) * tau + previous_time_line[i]
            alpha[i + 1] = B / (C - A * alpha[i])
            beta[i + 1] = (F + A * beta[i]) / (C - A * alpha[i])
        if right_bound_cond == 1:
            current_time_line[M] = fi_2(n * tau)
        else:
            current_time_line[M] = (-h * xi_2(n * tau) + beta[M]) / (
                1 - alpha[M]
            )
        for i in range(M - 1, -1, -1):
            current_time_line[i] = (
                alpha[i + 1] * current_time_line[i + 1] + beta[i + 1]
            )
        np.put(
            previous_time_line, [i for i in range(M + 1)], current_time_line
        )
        data_for_plot.append(current_time_line)
    print(current_time_line.tolist())
    if mode == 1:
        norm(current_time_line)


def norm(vector):
    C_abs = LA.norm(vector - right_answer, np.inf)
    I_abs = LA.norm(vector - right_answer) * math.sqrt(h)
    C_rel = C_abs / LA.norm(right_answer, np.inf)
    I_rel = I_abs / (LA.norm(right_answer) * math.sqrt(h))
    print("a) {}\nb) {}\nc) {}\nd) {}\n".format(C_abs, I_abs, C_rel, I_rel))


if __name__ == "__main__":

    # d U / d t = a^2 * (d^2 U / d x^2) + fun(t,x),  0 < x < 1
    # U(0,x) = u(x)                                  0 < t < t0
    # 1) U(t,0) = fi_1(t)  U(t,1) = fi_2(t)
    # 2) U'_x(t,0) = xi_1(t)  U'_x(t,1) = xi_2(t)

    # Boundary conditions First type or Second
    left_bound_cond = int(input("Input type of left boundary condition: "))
    right_bound_cond = int(input("Input type of right boundary condition: "))

    t0 = 1

    M = int(input("Input number of Coordinate nodes: "))
    N = int(input("Input number of Time nodes: "))

    h = float(t0 / M)
    tau = float(t0 / N)

    print(f"Coordinate step is { h } , Time step is { tau }")

    x_axis = np.linspace(0.0, math.pi, M + 1)
    t_axis = np.linspace(0.0, t0, N + 1)

    zero_time_line = np.vectorize(u_0)(x_axis)
    right_answer = np.vectorize(u)(x_axis)

    mode = int(input("1 - test, 2 - visualization: "))

    scheme = int(input("1 - explicit_scheme, 2 - implicit_scheme: "))

    data_for_plot = []
    if mode == 1:
        if scheme == 1:
            explicit_scheme()
        else:
            implicit_scheme()
    elif mode == 2:
        t0 = int(input('Input right boundary for t: '))
        tau = float(t0 / N)

        fig, ax = plt.subplots()
        line, = ax.plot([], [], lw=2)
        ax.grid()
        ax.set_xlim(0, t0)
        # ax.set_ylim(0, 0.002, auto=True)
        data_for_plot = []

        if scheme == 1:
            explicit_scheme()
        else:
            implicit_scheme()

        ani = anime.FuncAnimation(
            fig,
            run,
            data_for_plot,
            blit=False,
            interval=500,
            repeat=False,
            init_func=init,
        )
        plt.show()
        print(data_for_plot)
