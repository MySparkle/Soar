package edu.umich.soar.sproom;

public enum SharedNames {
	;
	private SharedNames() {
		throw new AssertionError();
	}

	public static final String DRIVE_CHANNEL = "DIFFERENTIAL_DRIVE_COMMAND";
	public static final String POSE_CHANNEL = "POSE";
	public static final String LASER_CHANNEL = "LIDAR_FRONT";
	
	// i/o links
	public static final String ABS_RELATIVE_BEARING = "abs-relative-bearing";
	public static final String ANGLE_RESOLUTION = "angle-resolution";
	public static final String ANGLE_UNITS = "angle-units";
	public static final String DISTANCE = "distance";
	public static final String FLOAT = "float";
	public static final String ID = "id";
	public static final String INT = "int";
	public static final String LENGTH_UNITS = "length-units";
	public static final String POSE = "pose";
	public static final String POSE_TRANSLATION = "pose_translation";
	public static final String RELATIVE_BEARING = "relative-bearing";
	public static final String WAYPOINT = "waypoint";
	public static final String X = "x";
	public static final String X_VELOCITY = "x-velocity";
	public static final String Y = "y";
	public static final String Y_VELOCITY = "y-velocity";
	public static final String YAW = "yaw";
	public static final String YAW_VELOCITY = "yaw-velocity";
	public static final String Z = "z";
	public static final String Z_VELOCITY = "z-velocity";
}