#include <iostream>
#include <ros/ros.h>

#include <std_msgs/Bool.h> // Message type for vision node for detection request
#include <cpp_publisher/Coordinates.h> // Message type for move node with coordinates of the block, target zone and block id
#include <cpp_publisher/BlockInfo.h> // Message type for vision node with block position, class and id
#include <cpp_publisher/MoveOperation.h> // Message type for move node with move operation result

#include <Eigen/Dense>
#include <vector>

#define DEBUG 1 // Set to 1 to test without vision
#define BLOCK_CLASSES 10 // Number of different block classes

using namespace std;
using Eigen::Vector3f;

void sendMoveOrder(Vector3f blockPos, int blockClass, int blockId); // Send move order to move node
void visionCallback(const cpp_publisher::BlockInfo::ConstPtr& msg); // Callback for vision node
void movementCallback(const cpp_publisher::MoveOperation::ConstPtr& msg); // Callback for move node

Vector3f getTargetZone(int blockClass); // Get the target zone for a block of a given class
bool isInWorkspace(Vector3f blockPos); // Check if a block is in the workspace

ros::Publisher movePublisher; // Publisher for move node
ros::Publisher visionPublisher; // Publisher for vision node
vector<int> blockPerClass(BLOCK_CLASSES, 0); // Number of blocks of each class

int main(int argc, char **argv)
{
    ros::init(argc, argv, "planner");
    ros::NodeHandle n;

    movePublisher = n.advertise<cpp_publisher::Coordinates>("/planner/position", 100);

    visionPublisher = n.advertise<std_msgs::Bool>("/planner/detection_request", 100);

    ros::Subscriber visionSubscriber = n.subscribe("/vision/vision_detection", 100, visionCallback);

    ros::Subscriber moveSubscriber = n.subscribe("/move/movement_results", 100, movementCallback);

    cout << "waiting for subscribers" << endl;
    
    if(!DEBUG){
        while(ros::ok()){
            if(visionPublisher.getNumSubscribers() > 0){
                std_msgs::Bool msg;
                msg.data = true;
                if(DEBUG)cout << "Publishing detection request" << endl;
                visionPublisher.publish(msg);
                break;
            }
            ros::spinOnce();
        }
    }else{
        while(ros::ok()){
            Vector3f blockPos;
            cout << "Enter block position" << endl;
            cin >> blockPos(0) >> blockPos(1) >> blockPos(2);
            int blockId = 1;
            int blockClass = 1;
            if(isInWorkspace(blockPos))
                sendMoveOrder(blockPos, blockClass, blockId);
        }
        
    }

    ros::spin();

    return 0;
}
/**
 * @brief Sends the move order to the move node, with the block position, class and id
 * 
 * @param blockPos 
 * @param blockClass 
 * @param blockId 
 */
void sendMoveOrder(Vector3f blockPos, int blockClass, int blockId){

    cout << "Sending move order" << endl;
    
    while(ros::ok()){
        cout << "Waiting for subscribers" << endl;
        if(movePublisher.getNumSubscribers() > 0){
            cout << "Publishing" << endl;
            cpp_publisher::Coordinates msg;

            std_msgs::Byte byteMsg;
            byteMsg.data = blockId;
            msg.blockId = byteMsg;

            msg.from.x = blockPos(0);
            msg.from.y = blockPos(1);
            msg.from.z = blockPos(2);

            Vector3f target = getTargetZone(blockClass);

            msg.to.x = target(0);
            msg.to.y = target(1);
            msg.to.z = target(2);

            if(msg.from.x < 0.5)movePublisher.publish(msg);
            
            break;
        }
    }
}

/**
 * @brief Get the target zone where to place a block of a given class
 * 
 * @param blockClass 
 * @return Vector3f 
 */
Vector3f getTargetZone(int blockClass){

    Vector3f target;

    float offset = 0.07 * blockPerClass[blockClass-1]; // 0.05 is the offset between blocks of same class

    blockPerClass[blockClass-1]+=1; // Increment the number of blocks of this class

    switch (blockClass)
    {
    case 1:
        target << 0.9, 0.5+offset, 0.9;
        break;
    case 2:
        target << 0.9, 0.5+offset, 0.9;
        break;
    case 3:
        target << 0.9, 0.5+offset, 0.9;
        break;
    case 4:
        target << 0.9, 0.5+offset, 0.9;
        break;
    case 5:
        target << 0.9, 0.5+offset, 0.9;
        break;
    case 6:
        target << 0.9, 0.5+offset, 0.9;
        break;
    case 7:
        target << 0.9, 0.5+offset, 0.9;
        break;
    case 8:
        target << 0.9, 0.5+offset, 0.9;
        break;
    case 9:
        target << 0.9, 0.5+offset, 0.9;
        break;
    case 10:
        target << 0.8, 0.5+offset, 0.9;
        break;
    }

    return target;
}

/**
 * @brief Given a block position, check if it is in the workspace (table)
 * 
 * @param blockPos 
 * @return true 
 * @return false 
 */
bool isInWorkspace(Vector3f blockPos){

    if(blockPos(0) > 0.05 && blockPos(0) < 0.5 && blockPos(1) > 0.05 && blockPos(1) < 0.75 && blockPos(2) > 0.86 && blockPos(2) < 0.92){
        return true;
    }
    return false;
}

/**
 * @brief Callback for vision node which receives the block position, class and id
 * 
 * @param msg 
 */
void visionCallback(const cpp_publisher::BlockInfo::ConstPtr& msg){
    
    cout << "Received vision callback" << endl;
    
    // Send move order
    Vector3f blockPos;
    blockPos << msg->blockPosition.x, msg->blockPosition.y, msg->blockPosition.z;
    int blockId = msg->blockId.data;
    int blockClass = msg->blockClass.data;

    if(isInWorkspace(blockPos))
        sendMoveOrder(blockPos, blockClass, blockId);

}

/**
 * @brief Callback for move node which receives the result of the movement
 * 
 * @param msg 
 */
void movementCallback(const cpp_publisher::MoveOperation::ConstPtr& msg){

    cout << "Received movement callback" << endl;

    cout << "Movement result: " << msg->result.data << endl;

    std_msgs::Bool boolMsg;
    boolMsg.data = true;
    visionPublisher.publish(msg->result);

}


