/* registration main:
 * Create 
 * - a Qt Application 
 * - a ROS Node, 
 * - a ROS listener, listening to and processing kinect data
 */

#include <ros/ros.h>
#include <rosbag/bag.h>
#include <rosbag/view.h>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/io.hpp>
#include <boost/foreach.hpp>
//#include <Eigen/Core>
#include <message_filters/subscriber.h>
#include <message_filters/time_synchronizer.h>
#include <stdint.h>
#include "pcl/ros/register_point_struct.h"
#include "pcl/kdtree/kdtree.h"
#include "pcl/kdtree/tree_types.h"
#include "pcl/io/pcd_io.h"
#include <string>
#include <pcl_ros/io/bag_io.h>

#include <sensor_msgs/Image.h>
#include <sensor_msgs/CameraInfo.h>
#include <message_filters/subscriber.h>
#include <message_filters/synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <sensor_msgs/CameraInfo.h>
#include <sensor_msgs/PointCloud2.h>
//#include <Eigen/Core>
#include <tf/transform_listener.h>
#include <pcl/filters/passthrough.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/features/normal_3d.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/filters/project_inliers.h>
#include <pcl/surface/convex_hull.h>
#include <pcl/segmentation/extract_polygonal_prism_data.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/filters/radius_outlier_removal.h>
#include "pcl/io/pcd_io.h"
#include "pcl/point_types.h"
#include "pcl/filters/statistical_outlier_removal.h"
#include "pcl/point_cloud.h"

void
transformPointCloud(boost::numeric::ublas::matrix<double> &transform, pcl::PointCloud<pcl::PointXYZRGB>::Ptr in,
                    pcl::PointCloud<pcl::PointXYZRGB>::Ptr out)
{

    boost::numeric::ublas::matrix<double> matIn(4, 1);
    *out = *in;

    for (size_t i = 0; i < in->points.size(); ++i)
    {
        double * matrixPtr = matIn.data().begin();

        matrixPtr[0] = in->points[i].x;
        matrixPtr[1] = in->points[i].y;
        matrixPtr[2] = in->points[i].z;
        matrixPtr[3] = 1;
        boost::numeric::ublas::matrix<double> matOut = prod(transform, matIn);
        matrixPtr = matOut.data().begin();

        out->points[i].x = matrixPtr[0];
        out->points[i].y = matrixPtr[1];
        out->points[i].z = matrixPtr[2];
    }
}


float
sqrG(float y)
{
    return y*y;
}

class VectorG
{
    double v[3];
public:

    VectorG()
    {
    }

    VectorG(double unitX, double unitY, double unitZ, bool normalize_ = false)
    {
        v[0] = unitX;
        v[1] = unitY;
        v[2] = unitZ;

        if (normalize_)
        {
            normalize();
        }

    }

    void
    normalize()
    {
        double norm = getNorm();
        for (int i = 0; i < 3; i++)
            v[i] = v[i] / norm;
    }

    double
    getNorm()
    {
        return sqrt(sqrG(v[0]) + sqrG(v[1]) + sqrG(v[2]));
    }

    double
    dotProduct(VectorG v2g)
    {
        double sum = 0;
        for (int i = 0; i < 3; i++)
            sum = sum + v[i] * v2g.v[i];
        return sum;
    }

    VectorG
    subtract(VectorG v2g)
    {
        VectorG out;
        for (int i = 0; i < 3; i++)
            out.v[i] = v[i] - v2g.v[i];
        return out;

    }

};

boost::numeric::ublas::matrix<double>
transformAsMatrix(const tf::Transform& bt)
{
    boost::numeric::ublas::matrix<double> outMat(4, 4);

    //  double * mat = outMat.Store();

    double mv[12];
    bt.getBasis().getOpenGLSubMatrix(mv);

    btVector3 origin = bt.getOrigin();

    outMat(0, 0) = mv[0];
    outMat(0, 1) = mv[4];
    outMat(0, 2) = mv[8];
    outMat(1, 0) = mv[1];
    outMat(1, 1) = mv[5];
    outMat(1, 2) = mv[9];
    outMat(2, 0) = mv[2];
    outMat(2, 1) = mv[6];
    outMat(2, 2) = mv[10];

    outMat(3, 0) = outMat(3, 1) = outMat(3, 2) = 0;
    outMat(0, 3) = origin.x();
    outMat(1, 3) = origin.y();
    outMat(2, 3) = origin.z();
    outMat(3, 3) = 1;


    return outMat;
}

class TransformG
{
public:
    boost::numeric::ublas::matrix<double> transformMat;

    TransformG(const tf::Transform& bt)
    {
        transformMat = transformAsMatrix(bt);
    }

    VectorG
    getXUnitVector()
    {
        return getIthColumn(0);
    }

    VectorG
    getZUnitVector()
    {
        return getIthColumn(2);
    }

    VectorG
    getIthColumn(int i)
    {
        return VectorG(transformMat(0,i), transformMat(1,i), transformMat(2,i));
    }

    VectorG
    getOrigin()
    {
        return VectorG(transformMat(0, 3), transformMat(1, 3), transformMat(2, 3));
    }

    double isPointVisible(VectorG vPoint)
    {
        VectorG cam2PointRay=vPoint.subtract(getOrigin());
        cam2PointRay.normalize();
        VectorG cam2PointRayUnit=cam2PointRay;
        double xDot=cam2PointRayUnit.dotProduct(getXUnitVector());
        double zDot=cam2PointRayUnit.dotProduct(getZUnitVector());
        std::cerr<<"dots"<<zDot<<","<<xDot<<","<<xDot/zDot<<std::endl;
        return xDot/zDot;
    }

    void
    print()
    {
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
                std::cerr << transformMat(i, j) << ",";
            std::cerr << std::endl;
        }
    }
};

btQuaternion
getQuaternion(geometry_msgs::Quaternion q)
{
    return btQuaternion(q.x, q.y, q.z, q.w);
}

btVector3
getVector3(geometry_msgs::Vector3 v)
{
    return btVector3(v.x, v.y, v.z);

}
typedef pcl::PointXYZRGB PointT;

void
applyFilters(pcl::PointCloud<pcl::PointXYZRGB>::Ptr inp_cloud_ptr, pcl::PointCloud<pcl::PointXYZRGB>::Ptr outp)
{
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB > ()), cloud_filtered(new pcl::PointCloud<pcl::PointXYZRGB > ());

    pcl::StatisticalOutlierRemoval<pcl::PointXYZRGB> sor;

    sor.setInputCloud(inp_cloud_ptr);
    std::cerr << "initially : " << inp_cloud_ptr->size() << std::endl;

    sor.setMeanK(50);
    sor.setStddevMulThresh(1.0);
    sor.filter(*cloud_filtered);

    pcl::RadiusOutlierRemoval<pcl::PointXYZRGB> ror;
    ror.setInputCloud(cloud_filtered);
    std::cerr << "before radius : " << cloud_filtered->size() << std::endl;
    ror.setRadiusSearch(0.01);
    ror.setMinNeighborsInRadius(2);
    ror.filter(*outp);
    std::cerr << "after radius : " << outp->size() << std::endl;
}

int
main(int argc, char** argv)
{
    //  ros::init(argc, argv,"hi");

    rosbag::Bag bag;
    std::cerr << "opening " << argv[1] << std::endl;
    bag.open(argv[1], rosbag::bagmode::Read);
    pcl::PointCloud<PointT>::Ptr final_cloud(new pcl::PointCloud<PointT > ()), cloud_filtered(new pcl::PointCloud<PointT > ());// cloud_transformed(new pcl::PointCloud<PointT > ());


    int tf_count = 0;
    int pcl_count = 0;

    std::vector<TransformG> transformsG;
    pcl_ros::BAGReader reader;
    if (!reader.open(argv[1], "/rgbdslam/my_clouds"))
    {
        ROS_ERROR("Couldn't read file ");
        return (-1);
    }
    sensor_msgs::PointCloud2ConstPtr cloud_blob, cloud_blob_prev;
    pcl::PointCloud<PointT> inp_cloud;

    pcl::PassThrough<PointT> pass_;
    do
    {
        cloud_blob_prev = cloud_blob;
        cloud_blob = reader.getNextCloud();
        ros::Time ptime = cloud_blob->header.stamp;

        if (cloud_blob_prev != cloud_blob)
        {
            pcl::fromROSMsg(*cloud_blob, inp_cloud);
            //ROS_INFO ("PointCloud with %d data points and frame %s (%f) received.", (int)cloud.points.size (), cloud.header.frame_id.c_str (), cloud.header.stamp.toSec ());
            pcl_count++;
        }

        //    rosbag::View view(bag, rosbag::TopicQuery("/rgbdslam/my_clouds"));


        rosbag::View view_tf(bag, rosbag::TopicQuery("/tf"), ptime - ros::Duration(0, 1), ptime + ros::Duration(0, 100000000));
        //std::cerr<<(view_tf.size())<<endl;
        std::cerr << ptime << std::endl;
        //        std::cerr<<"qid:"<<pcl_ptr->header.seq<<endl;;
        tf_count = 0;

        tf::Transform final_tft;

        BOOST_FOREACH(rosbag::MessageInstance const mtf, view_tf)
        {
            tf::tfMessageConstPtr tf_ptr = mtf.instantiate<tf::tfMessage > ();
            assert(tf_ptr != NULL);
            std::vector<geometry_msgs::TransformStamped> bt;
            tf_ptr->get_transforms_vec(bt);
            tf::Transform tft(getQuaternion(bt[0].transform.rotation), getVector3(bt[0].transform.translation));

            //  transG.print();
            if (ptime == bt[0].header.stamp)
            {
                tf_count++;
                std::cerr << "tf qid:" << bt[0].header.seq << std::endl;
                final_tft = tft;
            }
            assert(tf_count <= 1);
        }

        if (tf_count == 1)
        {
            TransformG transG(final_tft);
            pcl::PointCloud<PointT>::Ptr inp_cloud_ptr(new pcl::PointCloud<PointT > (inp_cloud));
            //  transformPointCloud(transG.transformMat,inp_cloud_ptr,cloud_transformed);
            // applyFilters(inp_cloud_ptr,cloud_filtered);
            std::cerr << "Origin" << inp_cloud.sensor_origin_[0] << "," << inp_cloud.sensor_origin_[1] << "," << inp_cloud.sensor_origin_[2] << "," << inp_cloud.sensor_origin_[3] << std::endl;
            transG.print();

            pass_.setInputCloud(inp_cloud_ptr);
            pass_.filter(*cloud_filtered);

            if (pcl_count == 1)
            {
                *final_cloud = *cloud_filtered;
                transformsG.push_back(transG);
                PointT cpoint;
                double mint=10000;
                double maxt=-10000;

                for (int p = 0; p < cloud_filtered->size(); p++)
                {
                    cpoint = cloud_filtered->points[p];
                    for (int c = 0; c < transformsG.size(); c++)
                    {
                        TransformG ctrans=transformsG[c];
                        VectorG vpointCam(cpoint.x,cpoint.y,cpoint.z,true);
                        double tt=ctrans.isPointVisible(vpointCam);
                        if(tt>maxt)
                            maxt=tt;

                        if(tt<mint)
                            mint=tt;



                    }
                }
                std::cerr<<"min-max"<<mint<<","<<maxt<<std::endl;
            }
            else if (pcl_count % 5 == 1)
            {
                //*final_cloud +=
            }
        }
        else
        {
            std::cerr << "no tf found";
        }



    }
    while (cloud_blob != cloud_blob_prev);

    applyFilters(final_cloud, cloud_filtered);

    bag.close();
    pcl::PCDWriter writer;

    writer.write<PointT > ("/home/aa755/VisibilityMerged.pcd", *cloud_filtered, false);

    //  ros::NodeHandle n;
    //Instantiate the kinect image listener
    /*  OpenNIListener kinect_listener(n,
    //  OpenNIListener kinect_listener(
                                     "/rgbdslam/batch_clouds",
                                     "/rgbdslam/my_clouds"
                                     );//,  "/camera/depth/image_raw");
 
       ros::spin();
     */

    //  while(1){
    //   sleep(20);

    //  }

}


