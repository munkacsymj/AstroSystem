import matplotlib.pyplot as plt

import xforms

def MakePlot(points, coefficient):
    x_array = [p.x_val for p in points]
    y_array = [p.y_val_adjusted for p in points]
    color_array = ['y' if p.exclude else 'k' for p in points]

    plt.scatter(x_array, y_array, c=color_array,s=2)
    y_label = coefficient.y_axis_pair[0] + '-' + coefficient.y_axis_pair[1]
    x_label = coefficient.x_axis_pair[0] + '-' + coefficient.x_axis_pair[1]
    plt.xlabel(x_label)
    plt.ylabel(y_label)
    plt.title(coefficient.name)
    plt.show()
    
