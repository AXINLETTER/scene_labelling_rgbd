#include "float.h"
#include "pcl/io/pcd_io.h"
#include "pcl/point_types.h"
#include "pcl/filters/passthrough.h"
#include "pcl/filters/extract_indices.h"
#include "pcl/features/intensity_spin.h"
#include "descriptors_3d/all_descriptors.h"
#include <point_cloud_mapping/kdtree/kdtree_ann.h>
#include "sensor_msgs/point_cloud_conversion.h"
//typedef pcl::PointXYGRGBCam PointT;
typedef pcl::PointXYZRGBCamSL PointT;

using namespace pcl;

void getMinMax(const pcl::PointCloud<PointT> &cloud, const pcl::PointIndices &indices, Eigen::Vector4f &min_p, Eigen::Vector4f &max_p) {
    min_p.setConstant(FLT_MAX);
    max_p.setConstant(-FLT_MAX);
    min_p[3] = max_p[3] = 0;

    for (size_t i = 0; i < indices.indices.size(); ++i) {
        if (cloud.points[indices.indices[i]].x < min_p[0]) min_p[0] = cloud.points[indices.indices[i]].x;
        if (cloud.points[indices.indices[i]].y < min_p[1]) min_p[1] = cloud.points[indices.indices[i]].y;
        if (cloud.points[indices.indices[i]].z < min_p[2]) min_p[2] = cloud.points[indices.indices[i]].z;

        if (cloud.points[indices.indices[i]].x > max_p[0]) max_p[0] = cloud.points[indices.indices[i]].x;
        if (cloud.points[indices.indices[i]].y > max_p[1]) max_p[1] = cloud.points[indices.indices[i]].y;
        if (cloud.points[indices.indices[i]].z > max_p[2]) max_p[2] = cloud.points[indices.indices[i]].z;
    }
}

void getMinMax(const pcl::PointCloud<PointT> &cloud, Eigen::Vector4f &min_p, Eigen::Vector4f &max_p) {
    min_p.setConstant(FLT_MAX);
    max_p.setConstant(-FLT_MAX);
    min_p[3] = max_p[3] = 0;

    for (size_t i = 0; i < cloud.points.size(); ++i) {
        if (cloud.points[i].x < min_p[0]) min_p[0] = cloud.points[i].x;
        if (cloud.points[i].y < min_p[1]) min_p[1] = cloud.points[i].y;
        if (cloud.points[i].z < min_p[2]) min_p[2] = cloud.points[i].z;

        if (cloud.points[i].x > max_p[0]) max_p[0] = cloud.points[i].x;
        if (cloud.points[i].y > max_p[1]) max_p[1] = cloud.points[i].y;
        if (cloud.points[i].z > max_p[2]) max_p[2] = cloud.points[i].z;
    }
}

void get_avg_feats(vector<vector<float> > &descriptor_results, vector<float> &avg_feats) {

    vector<vector<float> >::iterator it = descriptor_results.begin();
    while(it->size() == 0) it++;
    avg_feats.resize(it->size());


    int count = 0;
    for (vector<vector<float> >::iterator it = descriptor_results.begin(); it < descriptor_results.end(); it++) {
        if (it->size() > 0) {
            count++;
        }
        vector<float>::iterator i = avg_feats.begin();
        for (vector<float>::iterator it2 = it->begin(); it2 < it->end(); it2++, i++) {
            *i = *i + *it2;
        }

    }
    std::cerr << "average features" << std::endl;
    int c = 0;
    for (vector<float>::iterator i = avg_feats.begin(); i < avg_feats.end(); i++) {
        c++;
        *i = *i / count;
        std::cerr << c << " : " << *i << ",\t";
    }
    std::cerr << std::endl;
}

void concat_feats(vector<vector<float> > &avg_feats, vector<float> &features )
{
     for (vector<vector<float> >::iterator it = avg_feats.begin(); it < avg_feats.end(); it++) {

 //       vector<float>::iterator i = features.end();
        for (vector<float>::iterator it2 = it->begin(); it2 < it->end(); it2++) {
            features.push_back(*it2);
        }
    }
}

int main(int argc, char** argv) {

    sensor_msgs::PointCloud2 cloud_blob, cloud_tmp;
    sensor_msgs::PointCloud cloud_blob2;
    pcl::PointCloud<PointT> cloud;


    // read the pcd file

    if (pcl::io::loadPCDFile(argv[1], cloud_blob) == -1) {
        ROS_ERROR("Couldn't read file test_pcd.pcd");
        return (-1);
    }
    ROS_INFO("Loaded %d data points from test_pcd.pcd with the following fields: %s", (int) (cloud_blob.width * cloud_blob.height), pcl::getFieldsList(cloud_blob).c_str());

    // convert to templated message type

    pcl::fromROSMsg(cloud_blob, cloud);

    pcl::PointCloud<PointT>::Ptr cloud_ptr(new pcl::PointCloud<PointT > (cloud));

    pcl::PointCloud<PointT>::Ptr cloud_filtered(new pcl::PointCloud<PointT > ());
    pcl::PointCloud<PointT>::Ptr cloud_seg(new pcl::PointCloud<PointT > ());
    //pcl::PointCloud<PointXYZI>::Ptr cloud_seg (new pcl::PointCloud<PointXYZI> ());


    pcl::PointIndices::Ptr segment_indices(new pcl::PointIndices());

    // get segments

    // find the max segment number
    int max_segment_num = 0;
    for (size_t i = 0; i < cloud.points.size(); ++i) {
        if (max_segment_num < cloud.points[i].segment) {
            max_segment_num = (int) cloud.points[i].segment;
        }
    }


    ExtractIndices<PointT> extract;


    Eigen::Vector4f min_p;
    Eigen::Vector4f max_p;
    for (int seg = 1; seg <= max_segment_num; seg++) {
        vector<float> features;
        int label;
        segment_indices->indices.clear();
        for (size_t i = 0; i < cloud.points.size(); ++i) {
            if (cloud.points[i].segment == seg) {
                segment_indices->indices.push_back(i);
                label = cloud.points[i].label;
            }
        }
        extract.setInputCloud(cloud_ptr);
        extract.setIndices(segment_indices);
        extract.setNegative(false);
        extract.filter(*cloud_seg);
        std::cerr << seg << ". Cloud size after extracting : " << cloud_seg->points.size() << std::endl;
        if (cloud_seg->points.size() > 100) {
            SpectralAnalysis sa(0.05);
            SpinImageNormal spin_image(0.025, 0.025, 5, 4, false, sa);
            ShapeSpectral shape_spectral(sa);
            OrientationNormal o_normal(0, 0, 1, sa);
            OrientationTangent o_tangent(0, 0, 1, sa);
            Position position;
            BoundingBoxSpectral bbox_spectral(1.0, sa);
            vector<Descriptor3D*> descriptors_3d;
            descriptors_3d.push_back(&shape_spectral);
            descriptors_3d.push_back(&spin_image);
            descriptors_3d.push_back(&o_normal);
            descriptors_3d.push_back(&o_tangent);
            //descriptors_3d.push_back(&position);
            descriptors_3d.push_back(&bbox_spectral);

            pcl::toROSMsg(*cloud_seg, cloud_tmp);
            sensor_msgs::convertPointCloud2ToPointCloud(cloud_tmp, cloud_blob2);
            cloud_kdtree::KdTreeANN pt_cloud_kdtree(cloud_blob2);
            vector<const geometry_msgs::Point32*> interest_pts;
            if (cloud_seg->points.size() < 200) {
                interest_pts.resize(cloud_blob2.points.size());
                for (size_t i = 0; i < cloud_blob2.points.size(); i++) {
                    interest_pts[i] = &(cloud_blob2.points[i]);
                }
            } else {
                interest_pts.resize(100);
                map<int, int> in;
                int count = 0;
                while (count < 100) {
                    int a = rand() % cloud_blob2.points.size();
                    if (in.find(a) == in.end()) {
                        in[a] = 1;
                        interest_pts[count] = &(cloud_blob2.points[a]);
                    }
                    count++;
                }

            }


            //     vector<vector<float> >  descriptor_results;
            //     spin_image.compute(cloud_blob2, pt_cloud_kdtree, interest_pts, descriptor_results);
            unsigned int nbr_descriptors = descriptors_3d.size();
            vector<vector<vector<float> > > all_descriptor_results(nbr_descriptors);
            vector<vector<float> > avg_feats(nbr_descriptors);
            for (unsigned int i = 0; i < nbr_descriptors; i++) {
                descriptors_3d[i]->compute(cloud_blob2, pt_cloud_kdtree, interest_pts, all_descriptor_results[i]);
                
                std::cerr << "featnum: " << i << "\t"  ;
                get_avg_feats(all_descriptor_results[i], avg_feats[i]);
            }

            concat_feats(avg_feats,features);
            getMinMax(*cloud_ptr, *segment_indices, min_p, max_p);
            ROS_INFO("minp : %f,%f,%f\t maxp : %f,%f,%f", min_p[0], min_p[1], min_p[2], max_p[0], max_p[1], max_p[2]);
            ROS_INFO("size of all_descriptor_results : %d", all_descriptor_results[1].size());
            // add bounding box features
            features.push_back(max_p[0]-  min_p[0]);
            features.push_back(max_p[1]-  min_p[1]);
            features.push_back(max_p[2]-  min_p[2]);




        }
    }
    // for each segment compute features
    // feature 1 : height


}


