import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys

# Get filename from command line argument
if len(sys.argv) != 2:
    print("Usage: python script.py <csv_filename>")
    print("Example: python script.py 'optimal life percentage.csv'")
    sys.exit(1)

csv_filename = sys.argv[1]

# Read the CSV data
try:
    df = pd.read_csv(csv_filename)
except FileNotFoundError:
    print(f"Error: File '{csv_filename}' not found.")
    sys.exit(1)
except Exception as e:
    print(f"Error reading file '{csv_filename}': {e}")
    sys.exit(1)

# Convert density from string percentage to float
df["Density_Float"] = df["Density"].str.rstrip("%").astype(float)

# Get unique board sizes
board_sizes = sorted(df["Board Size"].unique())

# Set up the plotting style to match your reference chart
plt.style.use("default")

# Create a chart for each board size
for board_size in board_sizes:
    # Filter data for current board size
    size_data = df[df["Board Size"] == board_size].copy()
    size_data = size_data.sort_values("Density_Float")

    # Create the plot
    fig, ax = plt.subplots(figsize=(10, 6))

    # Plot P10 and Median lines
    ax.plot(
        size_data["Density_Float"],
        size_data["Median"],
        color="#4285F4",
        linewidth=2,
        label="Median",
        marker="o",
        markersize=3,
    )
    ax.plot(
        size_data["Density_Float"],
        size_data["P10"],
        color="#EA4335",
        linewidth=2,
        label="P10",
        marker="o",
        markersize=3,
    )

    # Find max values and their positions
    max_median_idx = size_data["Median"].idxmax()
    max_p10_idx = size_data["P10"].idxmax()

    max_median_density = size_data.loc[max_median_idx, "Density_Float"]
    max_median_value = size_data.loc[max_median_idx, "Median"]
    max_p10_density = size_data.loc[max_p10_idx, "Density_Float"]
    max_p10_value = size_data.loc[max_p10_idx, "P10"]

    # Add pale gray vertical lines at max values
    ax.axvline(
        x=max_median_density, color="lightgray", linestyle="--", alpha=0.7, linewidth=1
    )
    ax.axvline(
        x=max_p10_density, color="lightgray", linestyle="--", alpha=0.7, linewidth=1
    )

    # Add markers and labels for max values
    ax.plot(
        max_median_density,
        max_median_value,
        "o",
        color="#4285F4",
        markersize=8,
        markerfacecolor="white",
        markeredgewidth=2,
        zorder=5,
    )
    ax.plot(
        max_p10_density,
        max_p10_value,
        "o",
        color="#EA4335",
        markersize=8,
        markerfacecolor="white",
        markeredgewidth=2,
        zorder=5,
    )

    # Add density percentage labels
    ax.annotate(
        f"{max_median_density:.0f}%",
        xy=(max_median_density, max_median_value),
        xytext=(5, 10),
        textcoords="offset points",
        fontsize=10,
        fontweight="bold",
        color="#4285F4",
        bbox=dict(
            boxstyle="round,pad=0.3", facecolor="white", alpha=0.8, edgecolor="#4285F4"
        ),
    )

    ax.annotate(
        f"{max_p10_density:.0f}%",
        xy=(max_p10_density, max_p10_value),
        xytext=(5, -15),
        textcoords="offset points",
        fontsize=10,
        fontweight="bold",
        color="#EA4335",
        bbox=dict(
            boxstyle="round,pad=0.3", facecolor="white", alpha=0.8, edgecolor="#EA4335"
        ),
    )

    # Customize the plot
    ax.set_xlabel("Density (%)", fontsize=12)
    ax.set_ylabel("Generations Until Stable", fontsize=12)

    # Get sample size (assuming it's consistent for this board size)
    sample_size = size_data["Samples"].iloc[0]

    ax.set_title(
        f"Generations-Until-Stable for Various Alive-Percentage Random Board Fills\n{board_size}x{board_size} board size, {sample_size} samples",
        fontsize=14,
        color="#666666",
        pad=20,
    )

    # Set x-axis bounds based on actual data range
    x_min = size_data["Density_Float"].min()
    x_max = size_data["Density_Float"].max()
    ax.set_xlim(x_min, x_max)

    # Create x-axis ticks at reasonable intervals (multiples of 5 or 10)
    x_range = x_max - x_min
    if x_range <= 25:
        tick_interval = 5
    else:
        tick_interval = 10

    # Round to nearest appropriate interval and create tick range
    start_tick = int(np.ceil(x_min / tick_interval) * tick_interval)
    end_tick = int(np.floor(x_max / tick_interval) * tick_interval)
    x_ticks = np.arange(start_tick, end_tick + 1, tick_interval)
    ax.set_xticks(x_ticks)
    ax.set_xticklabels([f"{x}%" for x in x_ticks])

    # Set y-axis bounds based on data range with additional padding
    combined_data = np.concatenate([size_data["Median"], size_data["P10"]])
    y_min = combined_data.min()
    y_max = combined_data.max()
    y_range = y_max - y_min

    ax.set_ylim(max(0, y_min - y_range), y_max)

    # Add grid
    ax.grid(True, alpha=0.3, linestyle="-", linewidth=0.5)
    ax.set_axisbelow(True)

    # Add legend
    ax.legend(loc="upper right", frameon=True, fancybox=True, shadow=True)

    # Style the plot
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_color("#CCCCCC")
    ax.spines["bottom"].set_color("#CCCCCC")

    # Adjust layout
    plt.tight_layout()

    # Save the plot
    filename = f"game_of_life_{sample_size}_samples_{board_size}x{board_size}.png"
    plt.savefig(
        filename, dpi=300, bbox_inches="tight", facecolor="white", edgecolor="none"
    )

    print(f"Saved chart for board size {board_size} as {filename}")

    # Close the figure to free memory
    plt.close()

print(f"\nGenerated charts for board sizes: {board_sizes}")

# Optional: Create a summary statistics table
print("\nSummary Statistics:")
print("=" * 60)
for board_size in board_sizes:
    size_data = df[df["Board Size"] == board_size]
    max_median_row = size_data.loc[size_data["Median"].idxmax()]
    max_p10_row = size_data.loc[size_data["P10"].idxmax()]

    print(f"Board Size {board_size}:")
    print(
        f"  Highest Median: {max_median_row['Median']} at {max_median_row['Density']} density"
    )
    print(f"  Highest P10: {max_p10_row['P10']} at {max_p10_row['Density']} density")
    print()
