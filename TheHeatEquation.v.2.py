from numpy import linalg as LA
from typing import Callable
import matplotlib.pyplot as plt
import matplotlib.animation as anime
import numpy as np
import math
from scipy.linalg import solve_banded


class HeatEquationSolver:

    # domain size in space and time
    length = 1.0
    duration = 1.0

    # number of grid steps
    n_x = 100

    # number of time steps
    n_t = 100

    # heat diffusivity
    a = 1.0

    # boundary conditions
    left_bc_type = ""

    def left_bc(self, t):
        pass

    right_bc_type = ""

    def right_bc(self, t):
        pass

    # right-hand side
    def rhs(self, x, t):
        pass
    # Mode
    mode = ""

    # Analytical solution for test mode
    anl_solution = np.ndarray

    def __init__(self, l, d, n_x, n_t, a):
        self.length = l
        self.duration = d
        self.n_x = n_x
        self.n_t = n_t
        self.a = a
        self.h = self.length / (self.n_x - 1)
        self.tau = self.duration / (self.n_t - 1)
        self.x_grid = np.linspace(0, self.length, self.n_x)
        self.t_grid = np.linspace(0, self.duration, self.n_t)

    def set_left_bc(self, bc_type, func):
        self.left_bc_type = bc_type
        self.left_bc = func

    def set_right_bc(self, bc_type, func):
        self.right_bc_type = bc_type
        self.right_bc = func

    def set_rhs(self, func):
        self.rhs = func

    def set_mode(self, md):
        self.mode = md

    def set_anl_solution(self, u):
        self.anl_solution = np.vectorize(u)(self.x_grid, self.duration)

    def solve(self, initial_conditions, scheme):
        assert initial_conditions.size == self.n_x, "Wrong IC size"
        if scheme == "EXPLICIT":
            assert (
                self.tau < 0.5 * self.h ** 2 / self.a ** 2
            ), "Unstable scheme"
            return self._solve_explicit(initial_conditions)
        elif scheme == "IMPLICIT":
            return self._solve_implicit(initial_conditions)

    def _solve_explicit(self, initial_conditions):
        """Solve the heat equation using an explicit scheme. """
        coeff = self.a ** 2 * self.tau / self.h ** 2
        current_solution = initial_conditions
        next_solution = np.empty_like(current_solution)
        solutions = []

        for t in self.t_grid:
            next_solution[1:-1] = (
                current_solution[1:-1]
                + (current_solution[:-2] - 2 * current_solution[1:-1] + current_solution[2:]) * coeff
            ) + self.rhs(self.x_grid[1:-1], t) * self.tau

            # left bc
            if self.left_bc_type == "DIRICHLET":
                next_solution[0] = self.left_bc(t)
            elif self.left_bc_type == "NEUMANN":
                next_solution[0] = (
                    4 * next_solution[1]
                    - next_solution[2]
                    - 2 * self.h * self.left_bc(t)
                ) / 3.0

            # right bc
            if self.right_bc_type == "DIRICHLET":
                next_solution[-1] = self.right_bc(t)
            elif self.right_bc_type == "NEUMANN":
                next_solution[-1] = (
                    4 * next_solution[-2]
                    - next_solution[-3]
                    + 2 * self.h * self.right_bc(t)
                ) / 3.0
            if self.mode == "VISUALIZATION":
                solutions.append((t, next_solution.copy()))
            current_solution = next_solution
        if self.mode == "TEST":
            # print("Result:       ", current_solution.tolist())
            # print("Right answer: ", self.anl_solution.tolist())
            self._norma(current_solution)
        elif self.mode == "VISUALIZATION":
            return solutions

    def _solve_implicit(self, initial_conditions):
        """Solve the heat equation using an implicit scheme. """
        coeff = self.a ** 2 * self.tau / self.h ** 2
        l_and_u = (1, 1)
        ab = np.empty((3, self.n_x))
        # main diagonal
        ab[1] = 1 + 2.0 * coeff
        # upper and lower diagonals
        ab[0] = ab[2] = -coeff

        # left bc
        if self.left_bc_type == "DIRICHLET":
            ab[0][1] = 0  # upper diagonal
            ab[1][0] = 1  # main diagonal
        elif self.left_bc_type == "NEUMANN":
            ab[0][1] = 1  # upper diagonal
            ab[1][0] = -1  # main diagonal

        # right bc
        if self.right_bc_type == "DIRICHLET":
            ab[1][-1] = 1  # main diagonal
            ab[2][-2] = 0  # lower diagonal
        elif self.right_bc_type == "NEUMANN":
            ab[1][-1] = 1  # main diagonal
            ab[2][-2] = -1  # lower diagonal

        current_solution = initial_conditions
        solutions = []

        for t in self.t_grid:
            b = current_solution + self.tau * self.rhs(self.x_grid, t)
            # left bc
            if self.left_bc_type == "DIRICHLET":
                b[0] = self.left_bc(t)
            elif self.left_bc_type == "NEUMANN":
                b[0] = self.h * self.left_bc(t)
            # right bc
            if self.right_bc_type == "DIRICHLET":
                b[-1] = self.right_bc(t)
            elif self.right_bc_type == "NEUMANN":
                b[-1] = self.h * self.right_bc(t)

            next_solution = solve_banded(l_and_u, ab, b)
            if self.mode == "VISUALIZATION":
                solutions.append((t, next_solution.copy()))
            current_solution = next_solution
        if self.mode == "TEST":
            # print("Result:       ", current_solution.tolist())
            # print("Right answer: ", self.anl_solution.tolist())
            self._norma(current_solution)
        elif self.mode == "VISUALIZATION":
            return solutions

    def _norma(self, num_result):
        C_abs = LA.norm(self.anl_solution - num_result, np.inf)
        I_abs = LA.norm(self.anl_solution - num_result) * math.sqrt(self.h)
        C_rel = C_abs / LA.norm(self.anl_solution, np.inf)
        I_rel = I_abs / (LA.norm(self.anl_solution) * math.sqrt(self.h))
        print("a) {}\nb) {}\nc) {} %\nd) {} %\n".format(C_abs, I_abs, C_rel * 100, I_rel * 100))


if __name__ == "__main__":
    s = HeatEquationSolver(1.0, 4.0, 100, 400, 1.0)  # !!!!! Select boundary !!!!!!
    s.set_left_bc("NEUMANN", lambda t: 5*np.pi / 2 * (t**2 + 1)*np.cos(t))
    s.set_right_bc("NEUMANN", lambda t: 0)
    s.set_rhs(lambda x, t: (2 * t * np.cos(t) - (t**2 + 1) * np.sin(t) + 25 * np.pi**2 / 4 * (t**2 + 1)*np.cos(t)) * np.sin(5*np.pi / 2 * x))

    x = np.linspace(0, s.length, s.n_x)
    init_cond = np.sin(5 * np.pi / 2 * x)

    # main calculation
    mode = "VISUALIZATION"  # !!!!!! Select Mode !!!!!!
    s.set_mode(mode)
    if mode == "TEST":
        # !!!!!! Input Analytical Solution !!!!!!!
        s.set_anl_solution(lambda x, t: (t**2 + 1) * math.cos(t) * math.sin(5*math.pi / 2 * x))

    solutions = s.solve(init_cond, "IMPLICIT")  # !!!!!!! Select Scheme !!!!!!!

    if mode == "VISUALIZATION":
        # plot the results
        fig, ax = plt.subplots()
        num_line, = ax.plot([], [], lw=1, label="Numerical Solution")
        ax.grid()
        ax.set_xlim(0, s.length)
        ax.legend()

        def init():
            num_line.set_xdata(x)
            num_line.set_ydata(np.zeros_like(x))

            return num_line,

        count = 0

        def run(data):
            global count
            # update the data
            t, n_sol = data
            num_line.set_ydata(n_sol)
            if count == 0:
                ax.relim()
                ax.autoscale_view()
            ax.set_title("Iteration {}".format(count))
            count += 1
            return num_line,

        ani = anime.FuncAnimation(
            fig,
            run,
            solutions,
            blit=False,
            interval=50,
            repeat=False,
            init_func=init,
        )
        plt.show()
