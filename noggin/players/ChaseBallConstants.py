import BrunswickSpeeds as speeds
# Component Switches
USE_LOC_CHASE = True

# Transitions' Constants
# Ball on and off frame thresholds
BALL_ON_THRESH = 2
BALL_OFF_THRESH = 20
BALL_OFF_ACTIVE_LOC_THRESH = 200
# Value to stop spinning to ball and approach
BALL_APPROACH_BEARING_THRESH = 30
# Value to start spinning to ball
BALL_APPROACH_BEARING_OFF_THRESH = 40

# Should position for kick
BALL_POS_KICK_DIST_THRESH = 15.0
BALL_POS_KICK_BEARING_THRESH = 15
BALL_POS_KICK_LEFT_Y = 11.0
BALL_POS_KICK_RIGHT_Y = -BALL_POS_KICK_LEFT_Y
BALL_POS_KICK_MAX_X = 35
BALL_POS_KICK_MIN_X = 5

# States' constants
# turnToBall
FIND_BALL_SPIN_SPEED = speeds.MAX_SPIN_SPEED
BALL_SPIN_SPEED = speeds.MAX_SPIN_SPEED
BALL_SPIN_GAIN = 0.9
MIN_BALL_SPIN_MAGNITUDE = speeds.MIN_SPIN_MAGNITUDE

# approachBall() values
APPROACH_X_GAIN = 0.1
APPROACH_SPIN_SPEED = speeds.MAX_SPIN_WHILE_X_SPEED
MIN_APPROACH_SPIN_MAGNITUDE = speeds.MIN_SPIN_MAGNITUDE
APPROACH_SPIN_GAIN = 1.1
MAX_APPROACH_X_SPEED = speeds.MAX_X_SPEED
MIN_APPROACH_X_SPEED = speeds.MIN_X_SPEED

# approachBallWithLoc() values
IN_FRONT_SLOPE = 5.6
APPROACH_DIST_TO_BALL = 25
APPROACH_NO_LOC_THRESH = 4
APPROACH_OMNI_DIST = 25
APPROACH_ACTIVE_LOC_DIST = 40

# shouldKick()
BALL_KICK_LEFT_Y_L = 8
BALL_KICK_RIGHT_Y_R = -BALL_KICK_LEFT_Y_L
BALL_KICK_LEFT_Y_R = 6
BALL_KICK_LEFT_X_CLOSE = 2
BALL_KICK_LEFT_X_FAR = 14
POSITION_FOR_KICK_DIST_THRESH = 5
POSITION_FOR_KICK_BEARING_THRESH = 15
PFK_X_FAR = 25
PFK_X_CLOSE = 4


# Values for controlling the strafing
PFK_MAX_Y_SPEED = speeds.MAX_Y_SPEED
PFK_MIN_Y_SPEED = speeds.MIN_Y_SPEED
PFK_MAX_X_SPEED = speeds.MAX_X_SPEED
PFK_MIN_X_SPEED = speeds.MIN_X_MAGNITUDE
PFK_MIN_Y_MAGNITUDE = speeds.MIN_Y_MAGNITUDE
PFK_X_GAIN = 0.13
PFK_Y_GAIN = 0.6

# Keep track of what gait we're using
FAST_GAIT = "fastGait"
NORMAL_GAIT = "normalGait"

# Obstacle avoidance stuff
SHOULD_AVOID_OBSTACLE_APPROACH_DIST = 50.0
AVOID_OBSTACLE_FRONT_DIST = 40.0 #cm
AVOID_OBSTACLE_SIDE_DIST = 30.0 #cm
AVOID_OBSTACLE_FRAMES_THRESH = 2
DONE_AVOIDING_FRAMES_THRESH = 25
DODGE_BACK_SPEED = speeds.MIN_X_SPEED
DODGE_RIGHT_SPEED = speeds.MIN_Y_SPEED
DODGE_LEFT_SPEED = speeds.MAX_Y_SPEED

ORBIT_BALL_STEP_FRAMES = 150

TURN_LEFT = 1
TURN_RIGHT = -1

CHASE_AFTER_KICK_FRAMES = 100

# chaseAroundBox
GOALBOX_OFFSET = 10
STOP_CHASING_AROUND_BOX = 5

# find ball
WALK_TO_BALL_LOC_POS_FRAMES = 500
