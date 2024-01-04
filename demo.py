import numpy as np
import sysv_ipc
import matplotlib.pyplot as plt
import matplotlib.animation as animation

MATRIX_SIZE = 16

def main():
    key_demo = 2347

    # Create shared memory object
    memory = sysv_ipc.SharedMemory(key_demo)

    # Attach to shared memory
    memory.attach()

    # Calculate the expected size
    expected_size = MATRIX_SIZE * MATRIX_SIZE * np.dtype(int).itemsize

    # Print the expected size
    print(f"Expected Size: {expected_size}")

    # Create a numpy array from the shared memory
    matrix = np.frombuffer(memory.read(expected_size), dtype=int)

    # Print the size of the obtained array
    print(f"Obtained Array Size: {matrix.size}")

    # Reshape the array
    matrix = matrix.reshape((MATRIX_SIZE, MATRIX_SIZE))

    # Create a figure for the plot
    fig, ax = plt.subplots()

    # Initialize a matrix plot
    mat = ax.matshow(matrix)

    # Animation update function
    def update(i):
        # Update the data in the matrix plot
        mat.set_data(matrix)

    # Animate the plot
    ani = animation.FuncAnimation(fig, update, frames=10, repeat=True)

    # Show the plot
    plt.show()

if __name__ == "__main__":
    main()
