import torch
import pyzed.sl as sl
import numpy as np
import math
##
# Questo è solo un po' di lavoro iniziale per comprendere meglio le API di ZED
##
def initCamera():
    # Initialize camera
    print("Initializing camera...")
    zed = sl.Camera()

    # Set configuration parameters
    init_params = sl.InitParameters()
    init_params.sdk_verbose = False
    init_params.camera_resolution = sl.RESOLUTION.HD720
    init_params.camera_fps = 30
    init_params.depth_mode = sl.DEPTH_MODE.PERFORMANCE
    err = zed.open(init_params)

    # Try to open the camera
    if err != sl.ERROR_CODE.SUCCESS:
        print("An unexpected error occurred while opening the camera.")
        exit(1)
    
    zed_serial = zed.get_camera_information().serial_number
    print("Camera serial number: ", zed_serial)

    return zed
'''
This function retrieves only the image from the camera
@param camera: the zed camera object
'''
def captureImage(camera):
    image = sl.Mat()
    runtime_parameters = sl.RuntimeParameters()
    if (camera.grab() == sl.ERROR_CODE.SUCCESS):
        camera.retrieve_image(image, sl.VIEW.LEFT)
        timestamp = camera.get_timestamp(sl.TIME_REFERENCE.IMAGE)
        print("Image resolution : {0} x {1} || Image timestamp : {2}\n".format(image.get_width(), image.get_height(), timestamp))

'''
This function retrieves only the depth map from the camera
@param camera: the zed camera object
'''
def getDepth(camera):
    depth = sl.Mat()
    if (camera.grab() == sl.ERROR_CODE.success):
        camera.retrieve_measure(depth, sl.MEASURE.DEPTH)

'''
This function retrieves both the image and the depth map from the camera
@param camera: the zed camera object
'''
def getImageandDepth(camera):
    image = sl.Mat()
    depth = sl.Mat()
    if (camera.grab() == sl.ERROR_CODE.SUCCESS):
        camera.retrieve_image(image, sl.VIEW.LEFT)
        camera.retrieve_measure(depth, sl.MEASURE.DEPTH)
        timestamp = camera.get_timestamp(sl.TIME_REFERENCE.IMAGE)
        print("Image resolution : {0} x {1} || Image timestamp : {2}\n".format(image.get_width(), image.get_height(), timestamp))

'''
This function retrieves the depth of a certain point in the image
@param depth_map: the depth map of the image or the point cloud
@param point: the point in the image
@param depth_mode: the depth mode of the camera
'''
def getDistForPoint(depth_map, point, depth_mode):
    distance = -1
    if (sl.MEASURE.DEPTH):
        distance = depth_map.get_value(point[0], point[1])
    elif (sl.MEASURE.XYZRGBA):
        err, point_cloud_val = depth_map.get_value(point[0], point[1])
        distance = math.sqrt(point_cloud_val[0]**2, point_cloud_val[1]**2, point_cloud_val[2]**2)
    else :
        print("Error: Unknown depth mode")
    if (distance == -1):
        print("Error: Could not get distance for point")
    if np.isnan(distance):
        print("Error: Maybe you're too close to the object?")
    if np.isinf(distance):
        print("Error: Maybe you're too far from the object?")
    return distance

if __name__ == "__main__":
    zed = initCamera()
    captureImage(zed)
    getDepth(zed)
    getDistForPoint(zed, (0,0), sl.MEASURE.DEPTH)
    zed.close()