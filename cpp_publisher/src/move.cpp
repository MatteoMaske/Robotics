/**
 * @file move.cpp
 * @author Matteo Mascherin
 * @brief File containing the move node, which manage all the movement of the robot
 * @version 1.0
 * @date 2023-02-17
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <iostream>
#include <Eigen/Dense>
#include <cmath>

#include <ros/ros.h>
#include <std_msgs/Float64MultiArray.h>
#include <std_msgs/Byte.h>
#include <std_msgs/String.h>
#include <sensor_msgs/JointState.h> // Message type for joint states
#include <cpp_publisher/Coordinates.h> // Message type for move node with coordinates of the block, target zone and block id
#include <cpp_publisher/MoveOperation.h> // Message type for move node with the result of the movement
#include <ros_impedance_controller/generic_float.h>

#include "kinematicsUr5.cpp" // Kinematics of the UR5, used for inverse and forward kinematics
#include "frame2frame.cpp" // Functions for frame to frame transformations (world to EE)

///Flag to slow down the movement process
#define DEBUG 0
///Flag to enable the manual control of the robot
#define MANUAL_CONTROL 0
///Flag to enable the real robot mode
#define REAL_ROBOT 0

///Loop rate of publisher
#define LOOPRATE 1000
///Velocity of the movement while not approaching the block [m/s]
#define MOVEMENT_VELOCITY 0.3
///Velocity while approaching the block [m/s]
#define APPROACH_VELOCITY 0.1
///Number of joints of the robot
#define ROBOT_JOINTS 6
///Number of joints of the soft gripper
#define EE_SOFT_JOINTS 2
///Number of joints of the hard gripper
#define EE_HARD_JOINTS 3 

///Flag to enable the hard gripper
#define HARD_GRIPPER 1

using namespace std;
using Eigen::MatrixXf;
using Eigen::Vector3f;

//=======GLOBAL VARIABLES=======
///Publisher for desired joint state
ros::Publisher pub_des_jstate;
///Publisher for the result of the movement to be sent to the planner
ros::Publisher pub_move_operation;
///Client for the service call to move the gripper
ros::ServiceClient gripperClient;
///Current joint state of the robot
MatrixXf currentJoint(1,6);
///Current joint state of the gripper
MatrixXf currentGripper;

//=======FUNCTION DECLARATION=======
Vector3f xe(float t, Vector3f xef, Vector3f xe0, const float& movementTime); //linear interpolation of the position
MatrixXf toRotationMatrix(Vector3f euler); //convert euler angles to rotation matrix
void computeMovementDifferential(Vector3f targetPosition, Vector3f targetOrientation,float dt,const bool& approach);//compute the movement
MatrixXf invDiffKinematiControlComplete(MatrixXf q, MatrixXf xe, MatrixXf xd, MatrixXf vd, MatrixXf re, MatrixXf phif, MatrixXf kp, MatrixXf kphi);//compute qdot
MatrixXf computeOrientationError(MatrixXf wRe, MatrixXf wRd);//compute orientation error
MatrixXf jacobian(MatrixXf Th);//compute jacobian

void coordinateCallback(const cpp_publisher::Coordinates::ConstPtr& coordinateMessage);//callback for the coordinates
void publishJoint(MatrixXf publishPos); //publish the joint angles
void publishMoveOperation(int blockId, bool success); //publish the ack to planner
void changeSoftGripper(float firstVal, float secondVal); //change the soft gripper
void changeHardGripper(float diameter); //change the hard gripper
Vector3f mapToGripperJoints(float diameter); //map the diameter to the gripper joints

void moveObject(Vector3f pos, Vector3f ori, Vector3f targetPos); //move the object
void moveDown(float distance); //move down of distance
void moveUp(float distance); //move up of distance

void generateManualControlMenu(); //generate the manual control menu

//=======MAIN FUNCTION=======
int main(int argc, char **argv){

    //ROS initialization
    ros::init(argc, argv, "move");
    ros::NodeHandle node;

    pub_des_jstate = node.advertise<std_msgs::Float64MultiArray>("/ur5/joint_group_pos_controller/command", 1); //publisher for desired joint state

    pub_move_operation = node.advertise<cpp_publisher::MoveOperation>("/move/movement_results", 1); //publisher for desired joint state

    ros::Subscriber coordinateSubscriber = node.subscribe("/planner/position", 1, coordinateCallback); //subscriber for block position

    gripperClient = node.serviceClient<ros_impedance_controller::generic_float>("move_gripper");

    MatrixXf customHomingJoint(1,6);
    customHomingJoint <<   -2.7907,-0.78, -2.56,-1.63, -1.57, 3.49; //custom homing procedure joint angles

    /*initial gripper pos*/
    currentJoint = customHomingJoint;
    if(HARD_GRIPPER) {
        currentGripper.resize(1,3);
        currentGripper << 0.0, 0.0, 0.0;
    }else currentGripper.resize(1,2);

    float initialGripperDiameter = 130.0;
    changeHardGripper(initialGripperDiameter);

    //ros::spin() in order to wait for the planner to send the coordinates
    if(!MANUAL_CONTROL){
        while(ros::ok()){
            ros::spinOnce();
        }
    }

    //manual control of the robot with a menu
    if(MANUAL_CONTROL){
        generateManualControlMenu();
    }

    return 0;
}

//=======FUNCTION DEFINITION=======

/**
 * @brief Generate a human readable menu for manual control of the robot with differential commands
 * 
 */
void generateManualControlMenu(){
    int input;
    while(1){

        do{
            cout << "[1] for moving to a point with differential kinematics" << endl;
            cout << "[2] for getting current ee pos" << endl;
            cout << "[3] for getting current joint state" << endl;
            cout << "[4] for moving the gripper" << endl;
            cout << "[5] for moving up" << endl;
            cout << "[6] for moving down" << endl;
            cout << "[0] to exit" << endl;
            cin >> input;

        }while(input < 0 || input > 6);

        if(input == 1){

            Vector3f pos, ori;
            cout << "Insert the posistion coordinate: " << endl;
            cin >> pos(0) >> pos(1) >> pos(2);
            cout <<"Insert the orientation coordinate: " << endl;
            cin >> ori(0) >> ori(1) >> ori(2);

            cout <<"Choose the reference frame [0] world [1] end effector: " << endl;
            int refFrame;
            cin >> refFrame;

            if(refFrame == 0) {
                pos(2) += 0.01;
                pos = transformationWorldToBase(pos);
            }

            computeMovementDifferential(pos, ori ,0.001,false); //compute the movement to the first brick in tavolo_brick.world
        }else if(input==2){
            EEPose eePose;
            eePose = fwKin(currentJoint);
            cout << "Current ee position: " << endl;
            cout << eePose.Pe.transpose() << endl;
        }else if(input == 3){
            cout << "Current joint state: " << endl;
            cout << currentJoint << endl;
        }else if(input == 4){
            if(HARD_GRIPPER){
                float diameter;
                Vector3f ee_joints = Vector3f::Ones(3);
                cout << "Insert the value of the gripper joints:" << endl;
                cin >> diameter;
                changeHardGripper(diameter);
            }else{
                cout << "Insert the value of the gripper joints:" << endl;
                float value,value2;
                cin >> value >> value2;
                changeSoftGripper(value,value2);
            }
        }else if(input == 5){
            float height;
            cout << "Insert the height of the movement:" << endl;
            cin >> height;
            moveUp(height);
        }else if(input == 6){
            float height;
            cout << "Insert the height of the movement:" << endl;
            cin >> height;
            moveDown(height);
        }else{
            break;
        }
    }
}

/**
 * @brief Compute the movement using the differential kinematics and relying on a straight line trajectory, with a velocity switching either for approach or for movement
 *
 * @param targetPosition 
 * @param targetOrientation 
 * @param dt
 * @param approach
 */
void computeMovementDifferential(Vector3f targetPosition, Vector3f targetOrientation,float dt, const bool& approach){

    /*calc initial end effector pose*/
    EEPose eePose;
    eePose = fwKin(currentJoint);

    /*calc x0 and phie0*/
    Vector3f x0;
    x0 = eePose.Pe;

    Vector3f positionDifference;
    positionDifference = targetPosition - x0;
    float distance = positionDifference.norm();
    float movementTime;
    if(approach) movementTime = distance / APPROACH_VELOCITY;
    else movementTime = distance / MOVEMENT_VELOCITY;

    MatrixXf kp(3,3);
    kp = MatrixXf::Identity(3,3)*40;
    MatrixXf kphi(3,3);
    kphi = MatrixXf::Identity(3,3)*5;

    /*parameters for the loop*/
    Vector3f x;
    MatrixXf re(3,3);
    EEPose eePose1;

    MatrixXf qk(1,6);
    MatrixXf qk1(1,6);
    Vector3f vd;
    MatrixXf dotqk(6,6);
    Vector3f xArg;

    qk = currentJoint; //initialize qk

    for(float t=dt; t<=movementTime; t+=dt){

        eePose1 = fwKin(qk);
        x = eePose1.Pe;
        re = eePose1.Re;

        vd = (xe(t,targetPosition,x0,movementTime)-xe(t-dt,targetPosition,x0,movementTime)) / dt;
        xArg = xe(t,targetPosition,x0,movementTime);

        dotqk = invDiffKinematiControlComplete(qk,x,xArg,vd,re,targetOrientation,kp,kphi);
        qk1 = qk + dotqk.transpose()*dt; 
        qk = qk1;
       
        publishJoint(qk1);
    }

    currentJoint = qk1; //update current joint
}


/**
 * @brief Compute the joint velocities qdot using the inverse differential kinematics
 * 
 * @param q 
 * @param xe 
 * @param xd 
 * @param vd 
 * @param re 
 * @param phif 
 * @param kp 
 * @param kphi 
 * @return MatrixXf 
 */
MatrixXf invDiffKinematiControlComplete(MatrixXf q, MatrixXf xe, MatrixXf xd, MatrixXf vd, MatrixXf re, MatrixXf phif, MatrixXf kp, MatrixXf kphi){
    
    MatrixXf wRd(6,6);
    wRd = toRotationMatrix(phif);

    MatrixXf errorVector(3,1);
    errorVector = computeOrientationError(re,wRd);

    MatrixXf J(6,6);
    J = jacobian(q);
    
    MatrixXf dotQ(6,1);
    MatrixXf ve(6,1);

    float k = pow(10,-6); //dumping factor

    if(errorVector.norm() > 0.1){
        errorVector = 0.1*errorVector.normalized();
    }

    ve << (vd+kp*(xd-xe)), //kp correction factor ee pos
    (kphi*errorVector); //kphi corretion factor ee rot
    
    dotQ = (J+MatrixXf::Identity(6,6)*k).inverse()*ve;

    /*limit the velocity of the joints*/
    for(int i = 0; i < 6; i++){
        if(dotQ(i,0) > M_PI){
            dotQ(i,0) = 3.;
        }
        if(dotQ(i,0) < -M_PI){
            dotQ(i,0) = -3.;
        }
    }

    return dotQ;

}

/**
 * @brief Compute the orientation error between the desired and the current orientation
 * 
 * @param wRe 
 * @param wRd 
 * @return MatrixXf 
 */
MatrixXf computeOrientationError(MatrixXf wRe, MatrixXf wRd){
    
    MatrixXf relativeOrientation(3,3);
    relativeOrientation = wRe.transpose()*wRd;

    //compute the delta angle
    float cosDTheta = (relativeOrientation(0,0)+relativeOrientation(1,1)
                        +relativeOrientation(2,2)-1)/2;
    
    MatrixXf tmp(3,2);
    tmp << relativeOrientation(2,1),-relativeOrientation(1,2),
    relativeOrientation(0,2),-relativeOrientation(2,0),
    relativeOrientation(1,0),-relativeOrientation(0,1);

    float senDTheta = tmp.norm()/2;

    float dTheta = atan2(senDTheta,cosDTheta);

    MatrixXf aux(3,1);
    aux << relativeOrientation(2,1)-relativeOrientation(1,2),
            relativeOrientation(0,2)-relativeOrientation(2,0),
            relativeOrientation(1,0)-relativeOrientation(0,1);

    if(dTheta == 0){
        return MatrixXf::Zero(3,1);
    }else{
        MatrixXf axis(3,1);
        axis = (1/(2*senDTheta))*aux;
        return wRe * axis * dTheta;
    }
}

/**
 * @brief Calculate the jacobian matrix
 * 
 * @param Th 
 * @return MatrixXf 
 */
MatrixXf jacobian(MatrixXf Th){
    MatrixXf A(1,6);
    MatrixXf D(1,6);
    A << 0,-0.425,-0.3922,0,0,0;
    D << 0.1625,0,0,0.1333,0.0997,0.0996+0.14;

    MatrixXf J1(6,1);    
    J1 << D(4)*(cos(Th(0))*cos(Th(4)) + cos(Th(1)+Th(2)+Th(3))*sin(Th(0))*sin(Th(4))) + D(2)*cos(Th(0)) + D(3)*cos(Th(0)) - A(2)*cos(Th(1)+Th(2))*sin(Th(0)) - A(1)*cos(Th(1))*sin(Th(0)) - D(4)*sin(Th(1)+Th(2)+Th(3))*sin(Th(0)),
        D(4)*(cos(Th(4))*sin(Th(0)) - cos(Th(1)+Th(2)+Th(3))*cos(Th(0))*sin(Th(4))) + D(2)*sin(Th(0)) + D(3)*sin(Th(0)) + A(2)*cos(Th(1)+Th(2))*cos(Th(0)) + A(1)*cos(Th(0))*cos(Th(1)) + D(4)*sin(Th(1)+Th(2)+Th(3))*cos(Th(0)),
        0,
        0,
        0,
        1;
    

    MatrixXf J2(6,1);
    J2 << -cos(Th(0))*(A(2)*sin(Th(1)+Th(2)) + A(1)*sin(Th(1)) + D(4)*(sin(Th(1)+Th(2))*sin(Th(3)) - cos(Th(1)+Th(2))*cos(Th(3))) - D(4)*sin(Th(4))*(cos(Th(1)+Th(2))*sin(Th(3)) + sin(Th(1)+Th(2))*cos(Th(3)))),
        -sin(Th(0))*(A(2)*sin(Th(1)+Th(2)) + A(1)*sin(Th(1)) + D(4)*(sin(Th(1)+Th(2))*sin(Th(3)) - cos(Th(1)+Th(2))*cos(Th(3))) - D(4)*sin(Th(4))*(cos(Th(1)+Th(2))*sin(Th(3)) + sin(Th(1)+Th(2))*cos(Th(3)))),
        A(2)*cos(Th(1)+Th(2)) - (D(4)*sin(Th(1)+Th(2)+Th(3)+Th(4)))/2 + A(1)*cos(Th(1)) + (D(4)*sin(Th(1)+Th(2)+Th(3)-Th(4)))/2 + D(4)*sin(Th(1)+Th(2)+Th(3)),
        sin(Th(0)),
        -cos(Th(0)),
        0;


    MatrixXf J3(6,1);
    J3 << cos(Th(0))*(D(4)*cos(Th(1)+Th(2)+Th(3)) - A(2)*sin(Th(1)+Th(2)) + D(4)*sin(Th(1)+Th(2)+Th(3))*sin(Th(4))),
        sin(Th(0))*(D(4)*cos(Th(1)+Th(2)+Th(3)) - A(2)*sin(Th(1)+Th(2)) + D(4)*sin(Th(1)+Th(2)+Th(3))*sin(Th(4))),
        A(2)*cos(Th(1)+Th(2)) - (D(4)*sin(Th(1)+Th(2)+Th(3)+Th(4)))/2 + (D(4)*sin(Th(1)+Th(2)+Th(3)-Th(4)))/2 + D(4)*sin(Th(1)+Th(2)+Th(3)),
        sin(Th(0)),
        -cos(Th(0)),
        0;
    
    MatrixXf J4(6,1);
    J4 << D(4)*cos(Th(0))*(cos(Th(1)+Th(2)+Th(3)) + sin(Th(1)+Th(2)+Th(3))*sin(Th(4))),
        D(4)*sin(Th(0))*(cos(Th(1)+Th(2)+Th(3)) + sin(Th(1)+Th(2)+Th(3))*sin(Th(4))),
        D(4)*(sin(Th(1)+Th(2)+Th(3)-Th(4))/2 + sin(Th(1)+Th(2)+Th(3)) - sin(Th(1)+Th(2)+Th(3)+Th(4))/2),
        sin(Th(0)),
        -cos(Th(0)),
        0;

    MatrixXf J5(6,1);
    J5 << -D(4)*sin(Th(0))*sin(Th(4)) - D(4)*cos(Th(1)+Th(2)+Th(3))*cos(Th(0))*cos(Th(4)),
        D(4)*cos(Th(0))*sin(Th(4)) - D(4)*cos(Th(1)+Th(2)+Th(3))*cos(Th(4))*sin(Th(0)),
        -D(4)*(sin(Th(1)+Th(2)+Th(3)-Th(4))/2 + sin(Th(1)+Th(2)+Th(3)+Th(4))/2),
        sin(Th(1)+Th(2)+Th(3))*cos(Th(0)),
        sin(Th(1)+Th(2)+Th(3))*sin(Th(0)),
        -cos(Th(1)+Th(2)+Th(3));

    MatrixXf J6(6,1);
    J6 << 0,
        0,
        0,
        cos(Th(4))*sin(Th(0)) - cos(Th(1)+Th(2)+Th(3))*cos(Th(0))*sin(Th(4)),
        -cos(Th(0))*cos(Th(4)) - cos(Th(1)+Th(2)+Th(3))*sin(Th(0))*sin(Th(4)),
        -sin(Th(1)+Th(2)+Th(3))*sin(Th(4));

    MatrixXf J(6,6);
    J << J1, J2, J3, J4, J5, J6;
    
    return J;
}

/**
 * @brief From euler angles to rotation matrix
 * 
 * @param euler 
 * @return Eigen::MatrixXf 
 */
MatrixXf toRotationMatrix(Vector3f euler){
    Eigen::Matrix3f m;
    m = Eigen::AngleAxisf(euler(0), Eigen::Vector3f::UnitZ()) * Eigen::AngleAxisf(euler(1), Eigen::Vector3f::UnitY()) * Eigen::AngleAxisf(euler(2), Eigen::Vector3f::UnitX());
    return m;
}

/**
 * @brief Publish the joint angles to the robot, using its specific topic
 * 
 * @param publishPos
 */
void publishJoint(MatrixXf publishPos){

    std_msgs::Float64MultiArray msg;
    ros::Rate loop_rate(LOOPRATE);

    if(HARD_GRIPPER && !REAL_ROBOT){
        msg.data.resize(ROBOT_JOINTS+EE_HARD_JOINTS); //6 joint angles + 3 hard gripper angles
        msg.data.assign(ROBOT_JOINTS+EE_HARD_JOINTS,0); //empty the msg
        
        for (int i = 0; i < ROBOT_JOINTS; i++){
            msg.data[i] = publishPos(0, i);
        }
        for(int i=0; i<EE_HARD_JOINTS; i++){
            msg.data[i+ROBOT_JOINTS] = currentGripper(i);
        }

    }else if(REAL_ROBOT && HARD_GRIPPER){
        msg.data.resize(ROBOT_JOINTS); //6 joint angles
        msg.data.assign(ROBOT_JOINTS,0); //empty the msg

        for (int i = 0; i < ROBOT_JOINTS; i++){
            msg.data[i] = publishPos(0, i);
        }
    }

    pub_des_jstate.publish(msg); // publish the message

    loop_rate.sleep(); // sleep for the time remaining to let us hit our 1000Hz publish rate
}

/**
 * @brief Send an ack to the planner in order to communicate the correct execution of the move operation
 * 
 * @param blockId 
 * @param success 
 */
void publishMoveOperation(int blockId, bool success){

    cpp_publisher::MoveOperation msg;
    std_msgs::Byte byteMsg;
    std_msgs::String stringMsg;

    byteMsg.data = blockId;

    if(success){
        stringMsg.data = "success";
    }else{
        stringMsg.data = "fail - Something went wrong";
    }

    msg.blockId = byteMsg;
    msg.result = stringMsg;

    pub_move_operation.publish(msg);
}

/**
 * @brief Change the joint of the soft gripper, publishing to its topic
 * 
 * @param firstVal 
 * @param secondVal 
 */
void changeSoftGripper(float firstVal,float secondVal){

    ros::Rate loop_rate(LOOPRATE);

    std_msgs::Float64MultiArray msg;
    msg.data.resize(EE_SOFT_JOINTS); //6 joint angles + 3 end effector joints
    msg.data.assign(EE_SOFT_JOINTS,0); //empty the msg

    msg.data[0] = firstVal;
    msg.data[1] = secondVal;

    //pub_des_jstate.publish(msg); //to do -> change publisher with new topic

    loop_rate.sleep(); // sleep for the time remaining to let us hit our 1000Hz publish rate
}

/**
 * @brief Change the joint of the hard gripper, publishing to its topic
 * 
 * @param currentJoint 
 */
void changeHardGripper(float diameter){

    ros::Rate loop_rate(LOOPRATE);

    if(!REAL_ROBOT){

        Vector3f ee_joints = mapToGripperJoints(diameter);

        std_msgs::Float64MultiArray msg;
        msg.data.resize(ROBOT_JOINTS+EE_HARD_JOINTS); //6 joint angles + 3 end effector joints
        msg.data.assign(ROBOT_JOINTS+EE_HARD_JOINTS,0); //empty the msg

        for(int i = 0; i < ROBOT_JOINTS; i++)
            msg.data[i] = currentJoint(0,i);

        msg.data[ROBOT_JOINTS+0] = ee_joints(0);
        msg.data[ROBOT_JOINTS+1] = ee_joints(1);
        msg.data[ROBOT_JOINTS+2] = ee_joints(2);

        currentGripper = ee_joints;

        pub_des_jstate.publish(msg); //to do -> change publisher with new topic
    }else{
        ros_impedance_controller::generic_float srv;
        srv.request.data = diameter;
        if(gripperClient.call(srv)){
            cout << "Gripper call correctly sent" << endl;
        }else{
            cout << "Gripper call error!" << endl;
        }
    }
        

    loop_rate.sleep(); // sleep for the time remaining to let us hit our 1000Hz publish rate
}

/**
 * @brief This function computes the angles of the gripper joint based on the diameter given
 * 
 * @param diameter 
 * @return Vector3f 
 */
Vector3f mapToGripperJoints(float diameter){
    float alpha = (diameter - 22) / (130 - 22) * (-M_PI) + M_PI;
    Vector3f gripperJoints = Vector3f::Ones(3) * alpha;
    return gripperJoints;
}

/**
 * @brief The position to be reach at an instance t whilst moving from xe0 to xef (linear interpolation of the position)
 * 
 * @param t time elapsed so far
 * @param xef final position
 * @param xe0 initial position
 * @return Vector3f reprensenting the position
 */
Vector3f xe(float t, Vector3f xef, Vector3f xe0, const float& movementTime){
    Vector3f x;
    t = t / movementTime;
    x = t * xef + (1-t) * xe0;
    return x;
}

/**
 * @brief Callback function for the coordinates sended by the planner
 * 
 * @param coordinateMessage 
 */
void coordinateCallback(const cpp_publisher::Coordinates::ConstPtr& coordinateMessage){

    cout << "Received coordinates" << endl;

    cout << "Moving block " << coordinateMessage->blockId.data << endl;

    Vector3f pos,target;
    pos << coordinateMessage->from.x, coordinateMessage->from.y, coordinateMessage->from.z;
    target << coordinateMessage->to.x, coordinateMessage->to.y, coordinateMessage->to.z;

    Vector3f ori = Vector3f::Zero();

    //Adding 0.01 to the z coordinate to avoid collision with the table
    pos(2) = 0.92;
    target(2) = 0.92;

    cout << "Moving object from " << pos.transpose() << " to " << target.transpose() << endl;

    pos = transformationWorldToBase(pos);
    target = transformationWorldToBase(target);

    moveObject(pos, ori, target);

    cout << "Sending success message" << endl;
    publishMoveOperation(coordinateMessage->blockId.data, true);


}
/**
 * @brief Compute the movement routine to move the object from its position to its target position
 * 
 * @param pos 
 * @param ori 
 * @param targetPos 
 */
void moveObject(Vector3f pos, Vector3f ori, Vector3f targetPos){

    EEPose eePose;

    // Kinematics
    cout << "Starting kinematics" << endl;

    //Moving above the block
    cout << "Moving above the block" << endl;
    Vector3f tmp = pos;
    tmp(2) -= 0.2;
    computeMovementDifferential(tmp, ori, 0.001,false);
    if(DEBUG)sleep(2);

    //moving in z
    cout << "Moving in z" << endl;
    computeMovementDifferential(pos, ori, 0.001,true);
    if(DEBUG)sleep(2);

    // Grasping
    cout << "Grasping object" << endl;
    Vector3f gripperJoints;
    float diameter=60;
    if(!REAL_ROBOT){
        diameter = 40;
    }
    changeHardGripper(diameter);
    sleep(2);

    //moving in z
    cout << "Moving in z" << endl;
    moveUp(0.1);
    if(DEBUG)sleep(2);

    //moving in the left check point to stay safe
    cout << "Moving to the left check point" << endl;
    eePose = fwKin(currentJoint);
    tmp(0) = -0.4;
    tmp(1) = -0.4;
    tmp(2) = 0.5;
    computeMovementDifferential(tmp, Vector3f::Zero(), 0.001,false);
    if(DEBUG)sleep(2);

    //moving to the right check point to stay safe
    cout << "Moving to the right check point" << endl;
    tmp(0) = 0.4;
    tmp(1) = -0.4;
    tmp(2) = 0.5;
    computeMovementDifferential(tmp, Vector3f::Zero(), 0.001,false);
    if(DEBUG)sleep(2);

    //moving in x,y
    tmp = targetPos;
    eePose = fwKin(currentJoint);
    tmp(2) = eePose.Pe(2);
    computeMovementDifferential(tmp, Vector3f::Zero(), 0.001,false);
    if(DEBUG)sleep(2);

    // Moving to target in z
    cout << "Moving to target" << endl;
    computeMovementDifferential(targetPos, Vector3f::Zero(), 0.001,true);
    if(DEBUG)sleep(2);

    // Releasing
    cout << "Releasing object" << endl;
    changeHardGripper(100);
    sleep(2);

    // Moving up
    cout << "Moving up" << endl;
    moveUp(0.2);

    // Move in a safe position to take the next object
    cout << "Moving in a safe position, waiting for other objects" << endl;
    eePose = fwKin(currentJoint);
    Vector3f currentPos = eePose.Pe;
    if(currentPos(1)>-0.4){
        eePose.Pe(1) = -0.4;
        computeMovementDifferential(eePose.Pe, Vector3f::Zero(), 0.001,false);
    }
    if(DEBUG)sleep(2);

    // Moving back in the left of the table
    tmp = Vector3f(0.2, 0.8, 1.1);
    tmp = transformationWorldToBase(tmp);
    computeMovementDifferential(tmp, Vector3f::Zero(), 0.001,false);
    moveUp(0.2);
    if(DEBUG)sleep(2);
    
}

/**
 * @brief Move the robot up by a certain distance on the z axis
 * 
 * @param distance 
 */
void moveUp(float distance){

    EEPose eepose = fwKin(currentJoint);
    Vector3f target = eepose.Pe;
    target(2) -= distance;

    computeMovementDifferential(target, eepose.Re.eulerAngles(2,1,0), 0.001,true);
}

/**
 * @brief Move the robot down by a certain distance on the z axis
 * 
 * @param distance 
 */
void moveDown(float distance){

    EEPose eepose = fwKin(currentJoint);
    Vector3f target = eepose.Pe;
    target(2) += distance;

    computeMovementDifferential(target, eepose.Re.eulerAngles(2,1,0), 0.001,true);
}
