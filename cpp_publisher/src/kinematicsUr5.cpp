/**
 * @file kinematicsUr5.cpp
 * @author Stefano Sacchet
 * @brief File containing the functions to calculate the forward and inverse kinematics of the UR5 robot
 * @version 1.0
 * @date 2023-02-17
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <iostream>
#include <Eigen/Dense>
#include <cmath>
#include <iomanip>

using namespace std;
using Eigen::MatrixXf;

//distance vectors
///Length of the common normal between the z-axes of consecutive joints following the Denavit-Hartenberg convention
const float A[6] = {0, -0.425, -0.3922, 0, 0, 0};
///Distance between the z-axes of consecutive joints following the Denavit-Hartenberg convention
const float D[6] = {0.1625, 0, 0, 0.1333, 0.0997, 0.0996+0.14};
/**
 * @brief Struct to store the position and orientation of the end effector
 * 
 */
struct EEPose{
    Eigen::Vector3f Pe;
    Eigen::Matrix3f Re;
};

EEPose fwKin(MatrixXf Th); // This function will calculate the forward kinematics of the robot and return the position of the end effector
MatrixXf invKin(EEPose eePose); // This function will calculate the inverse kinematics of the robot and return the joint angles

//calculates rotation matrix for each joint
MatrixXf calcA10(float th0);
MatrixXf calcA21(float th1);
MatrixXf calcA32(float th2);
MatrixXf calcA43(float th3);
MatrixXf calcA54(float th4);
MatrixXf calcA65(float th5);

MatrixXf calcA10(float Th0){
    MatrixXf A10(4,4);

    A10 <<  cos(Th0), -sin(Th0), 0, 0,
            sin(Th0), cos(Th0), 0, 0,
            0, 0, 1, D[0],
            0, 0, 0, 1;

    return A10;
}

MatrixXf calcA21(float Th1){
    MatrixXf A21(4,4);

    A21 <<  cos(Th1), -sin(Th1), 0, 0,
            0, 0, -1, 0,
            sin(Th1), cos(Th1), 0, 0,
            0, 0, 0, 1;

    return A21;
}

MatrixXf calcA32(float Th2){
    MatrixXf A32(4,4);

    A32 <<  cos(Th2), -sin(Th2), 0, A[1],
            sin(Th2), cos(Th2), 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1;

    return A32;
}

MatrixXf calcA43(float Th3){
    MatrixXf A43(4,4);

    A43 <<  cos(Th3), -sin(Th3), 0, A[2],
            sin(Th3), cos(Th3), 0, 0,
            0, 0, 1, D[3],
            0, 0, 0, 1;

    return A43;
}

MatrixXf calcA54(float Th4){
    MatrixXf A54(4,4);

    A54 <<  cos(Th4), -sin(Th4), 0, 0,
            0, 0, -1, -D[4],
            sin(Th4), cos(Th4), 0, 0,
            0, 0, 0, 1;

    return A54;
}

MatrixXf calcA65(float Th5){
    MatrixXf A65(4,4);

    A65 <<  cos(Th5), -sin(Th5), 0, 0,
            0, 0, 1, D[5],
            -sin(Th5), -cos(Th5), 0, 0,
            0, 0, 0, 1;

    return A65;
}
/**
 * @brief This function will calculate the forward kinematics of the robot and return the position of the end effector
 * 
 * @param Th 
 * @return EEPose 
 */
EEPose fwKin(MatrixXf Th){

    MatrixXf A60(4, 4);
    MatrixXf Re(3,3);
    MatrixXf Pe(1,3);

    A60 = calcA10(Th(0)) * calcA21(Th(1)) * calcA32(Th(2)) * calcA43(Th(3)) * calcA54(Th(4)) * calcA65(Th(5));

    Pe = A60.block(0,3,3,1);
    Re = A60.block(0,0,3,3);

    EEPose eePose;
    eePose.Pe = Pe;
    eePose.Re = Re;

    return eePose;
}

/**
 * @brief This function will calculate the inverse kinematics of the robot and return the 8 possible joint configurations
 * 
 * @param eePose 
 * @return MatrixXf 
 */
MatrixXf invKin(EEPose eePose){

    MatrixXf Re = eePose.Re;
    MatrixXf T60(4, 4);
    float endEffectorPos[3] = {eePose.Pe(0), eePose.Pe(1), eePose.Pe(2)};

    T60 << Re(0, 0), Re(0, 1), Re(0, 2), endEffectorPos[0],
        Re(1, 0), Re(1, 1), Re(1, 2), endEffectorPos[1],
        Re(2, 0), Re(2, 1), Re(2, 2), endEffectorPos[2],
        0, 0, 0, 1;

    //Computation values for th1
    MatrixXf p50(1, 4);
    MatrixXf temp(4, 1);
    temp << 0, 0, -D[5], 1;

    p50 = T60 * temp;

    complex<float> th1_1 = real(atan2(p50(1, 0), p50(0, 0)) + real(acos(D[3] / hypot(p50(1, 0), p50(0, 0)))) + M_PI_2);
    complex<float> th1_2 = real(atan2(p50(1, 0), p50(0, 0)) - real(acos(D[3] / hypot(p50(1, 0), p50(0, 0)))) + M_PI_2);

    //Computation values for th5
    complex<float> th5_1 = real(acos((endEffectorPos[0] * sin(th1_1) - endEffectorPos[1] * cos(th1_1) - D[3]) / D[5]));
    complex<float> th5_2 = real(-acos((endEffectorPos[0] * sin(th1_1) - endEffectorPos[1] * cos(th1_1) - D[3]) / D[5]));
    complex<float> th5_3 = real(acos((endEffectorPos[0] * sin(th1_2) - endEffectorPos[1] * cos(th1_2) - D[3]) / D[5]));
    complex<float> th5_4 = real(-acos((endEffectorPos[0] * sin(th1_2) - endEffectorPos[1] * cos(th1_2) - D[3]) / D[5]));

    //Computation values for th6
    // related to th11 a th51
    MatrixXf T06(4, 4);
    MatrixXf Xhat(3, 1);
    MatrixXf Yhat(3, 1);

    T06 = T60.inverse();
    Xhat = T06.block(0, 0, 3, 1);
    Yhat = T06.block(0, 1, 3, 1);

    complex<float> th6_1 = real(atan2(((-Xhat(1) * real(sin(th1_1)) + Yhat(1) * real(cos(th1_1))) / real(sin(th5_1))), ((Xhat(0) * real(sin(th1_1)) - Yhat(0) * real(cos(th1_1))) / real(sin(th5_1)))));
    // related to th11 a th52
    complex<float> th6_2 = real(atan2(((-Xhat(1) * real(sin(th1_1)) + Yhat(1) * real(cos(th1_1))) / real(sin(th5_2))), ((Xhat(0) * real(sin(th1_1)) - Yhat(0) * real(cos(th1_1))) / real(sin(th5_2)))));

    // related to th12 a th53
    complex<float> th6_3 = real(atan2(((-Xhat(1) * real(sin(th1_2)) + Yhat(1) * real(cos(th1_2))) / real(sin(th5_3))), ((Xhat(0) * real(sin(th1_2)) - Yhat(0) * real(cos(th1_2))) / real(sin(th5_3)))));
    // related to th12 a th54
    complex<float> th6_4 = real(atan2(((-Xhat(1) * real(sin(th1_2)) + Yhat(1) * real(cos(th1_2))) / real(sin(th5_4))), ((Xhat(0) * real(sin(th1_2)) - Yhat(0) * real(cos(th1_2))) / real(sin(th5_4)))));

    MatrixXf T41m(4, 4);
    MatrixXf p41_1(1, 4);
    MatrixXf p41_2(1, 4);
    MatrixXf p41_3(1, 4);
    MatrixXf p41_4(1, 4);

    //------------------------
    //cout << calcA10(th1_1) << endl;

    T41m = calcA10(real(th1_1)).inverse() * T60 * calcA65(real(th6_1)).inverse() * calcA54(real(th5_1)).inverse();
    p41_1 = T41m.block(0, 3, 3, 1);
    float p41xz_1 = hypot(p41_1(0), p41_1(2));

    T41m = calcA10(real(th1_1)).inverse() * T60 * calcA65(real(th6_2)).inverse() * calcA54(real(th5_2)).inverse();
    p41_2 = T41m.block(0, 3, 3, 1);
    float p41xz_2 = hypot(p41_2(0), p41_2(2));

    T41m = calcA10(real(th1_2)).inverse() * T60 * calcA65(real(th6_3)).inverse() * calcA54(real(th5_3)).inverse();
    p41_3 = T41m.block(0, 3, 3, 1);
    float p41xz_3 = hypot(p41_3(0), p41_3(2));

    T41m = calcA10(real(th1_2)).inverse() * T60 * calcA65(real(th6_4)).inverse() * calcA54(real(th5_4)).inverse();
    p41_4 = T41m.block(0, 3, 3, 1);
    float p41xz_4 = hypot(p41_4(0), p41_4(2));

    //Computation of the 8 possible values for th3
    complex<float> th3_1 = real(acos((pow(p41xz_1, 2) - pow(A[1], 2) - pow(A[2], 2)) / (2 * A[1] * A[2])));
    complex<float> th3_2 = real(acos((pow(p41xz_2, 2) - pow(A[1], 2) - pow(A[2], 2)) / (2 * A[1] * A[2])));
    complex<float> th3_3 = real(acos((pow(p41xz_3, 2) - pow(A[1], 2) - pow(A[2], 2)) / (2 * A[1] * A[2])));
    complex<float> th3_4 = real(acos((pow(p41xz_4, 2) - pow(A[1], 2) - pow(A[2], 2)) / (2 * A[1] * A[2])));
    
    complex<float> th3_5 = -th3_1;
    complex<float> th3_6 = -th3_2;
    complex<float> th3_7 = -th3_3;
    complex<float> th3_8 = -th3_4;

    complex<float> th2_1 = real(atan2(-p41_1(2), -p41_1(0))) - real(asin((-A[2] * real(sin(th3_1))) / p41xz_1));
    complex<float> th2_2 = real(atan2(-p41_2(2), -p41_2(0))) - real(asin((-A[2] * real(sin(th3_2))) / p41xz_2));
    complex<float> th2_3 = real(atan2(-p41_3(2), -p41_3(0))) - real(asin((-A[2] * real(sin(th3_3))) / p41xz_3));
    complex<float> th2_4 = real(atan2(-p41_4(2), -p41_4(0))) - real(asin((-A[2] * real(sin(th3_4))) / p41xz_4));

    complex<float> th2_5 = real(atan2(-p41_1(2), -p41_1(0))) - real(asin((A[2] * real(sin(th3_1))) / p41xz_1));
    complex<float> th2_6 = real(atan2(-p41_2(2), -p41_2(0))) - real(asin((A[2] * real(sin(th3_2))) / p41xz_2));
    complex<float> th2_7 = real(atan2(-p41_3(2), -p41_3(0))) - real(asin((A[2] * real(sin(th3_3))) / p41xz_3));
    complex<float> th2_8 = real(atan2(-p41_4(2), -p41_4(0))) - real(asin((A[2] * real(sin(th3_4))) / p41xz_4));

    //Computation of the 8 possible value for th4
    MatrixXf T43m(4,4);
    MatrixXf Xhat43(1,4);

    T43m = calcA32(real(th3_1)).inverse() * calcA21(real(th2_1)).inverse() * calcA10(real(th1_1)).inverse() * T60 * calcA65(real(th6_1)).inverse() * calcA54(real(th5_1)).inverse();
    Xhat43 = T43m.block(0, 0, 3, 1);
    float th4_1 = atan2(Xhat43(1), Xhat43(0));

    T43m = calcA32(real(th3_2)).inverse() * calcA21(real(th2_2)).inverse() * calcA10(real(th1_1)).inverse() * T60 * calcA65(real(th6_2)).inverse() * calcA54(real(th5_2)).inverse();
    Xhat43 = T43m.block(0, 0, 3, 1);
    float th4_2 = atan2(Xhat43(1), Xhat43(0));

    T43m = calcA32(real(th3_3)).inverse() * calcA21(real(th2_3)).inverse() * calcA10(real(th1_2)).inverse() * T60 * calcA65(real(th6_3)).inverse() * calcA54(real(th5_3)).inverse();
    Xhat43 = T43m.block(0, 0, 3, 1);
    float th4_3 = atan2(Xhat43(1), Xhat43(0));

    T43m = calcA32(real(th3_4)).inverse() * calcA21(real(th2_4)).inverse() * calcA10(real(th1_2)).inverse() * T60 * calcA65(real(th6_4)).inverse() * calcA54(real(th5_4)).inverse();
    Xhat43 = T43m.block(0, 0, 3, 1);
    float th4_4 = atan2(Xhat43(1), Xhat43(0));

    T43m = calcA32(real(th3_5)).inverse() * calcA21(real(th2_5)).inverse() * calcA10(real(th1_1)).inverse() * T60 * calcA65(real(th6_1)).inverse() * calcA54(real(th5_1)).inverse();
    Xhat43 = T43m.block(0, 0, 3, 1);
    float th4_5 = atan2(Xhat43(1), Xhat43(0));

    T43m = calcA32(real(th3_6)).inverse() * calcA21(real(th2_6)).inverse() * calcA10(real(th1_1)).inverse() * T60 * calcA65(real(th6_2)).inverse() * calcA54(real(th5_2)).inverse();
    Xhat43 = T43m.block(0, 0, 3, 1);
    float th4_6 = atan2(Xhat43(1), Xhat43(0));

    T43m = calcA32(real(th3_7)).inverse() * calcA21(real(th2_7)).inverse() * calcA10(real(th1_2)).inverse() * T60 * calcA65(real(th6_3)).inverse() * calcA54(real(th5_3)).inverse();
    Xhat43 = T43m.block(0, 0, 3, 1);
    float th4_7 = atan2(Xhat43(1), Xhat43(0));

    T43m = calcA32(real(th3_8)).inverse() * calcA21(real(th2_8)).inverse() * calcA10(real(th1_2)).inverse() * T60 * calcA65(real(th6_4)).inverse() * calcA54(real(th5_4)).inverse();
    Xhat43 = T43m.block(0, 0, 3, 1);
    float th4_8 = atan2(Xhat43(1), Xhat43(0));

    //Reuslt of the inverse kinematics
    MatrixXf Th(8,6);

    Th << real(th1_1), real(th2_1), real(th3_1), real(th4_1), real(th5_1), real(th6_1),
          real(th1_1), real(th2_2), real(th3_2), real(th4_2), real(th5_2), real(th6_2),
          real(th1_2), real(th2_3), real(th3_3), real(th4_3), real(th5_3), real(th6_3),
          real(th1_2), real(th2_4), real(th3_4), real(th4_4), real(th5_4), real(th6_4),
          real(th1_1), real(th2_5), real(th3_5), real(th4_5), real(th5_1), real(th6_1),
          real(th1_1), real(th2_6), real(th3_6), real(th4_6), real(th5_2), real(th6_2),
          real(th1_2), real(th2_7), real(th3_7), real(th4_7), real(th5_3), real(th6_3),
          real(th1_2), real(th2_8), real(th3_8), real(th4_8), real(th5_4), real(th6_4);

    return Th;
}
