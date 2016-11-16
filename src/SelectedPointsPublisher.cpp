#include "rviz/selection/selection_manager.h"
#include "rviz/viewport_mouse_event.h"
#include "rviz/display_context.h"
#include "rviz/selection/forwards.h"
#include "rviz/properties/property_tree_model.h"
#include "rviz/properties/property.h"
#include "rviz/properties/vector_property.h"
#include "rviz/view_manager.h"
#include "rviz/view_controller.h"
#include "OGRE/OgreCamera.h"


#include "selected_points_publisher/SelectedPointsPublisher.h"

#include <ros/ros.h>
#include <ros/time.h>
#include <sensor_msgs/PointCloud2.h>
#include <QVariant>

#include <pcl_conversions/pcl_conversions.h>
#include <pcl/common/transforms.h>

#include <pcl/visualization/pcl_visualizer.h>

#include <pcl/filters/passthrough.h>
#include <pcl/common/common.h>

#include <pcl/filters/impl/box_clipper3D.hpp>


#include <visualization_msgs/Marker.h>

#include <pcl/filters/crop_box.h>
#include <pcl/common/angles.h>

namespace rviz_plugin_selected_points_publisher
{
SelectedPointsPublisher::SelectedPointsPublisher()
{
    updateTopic();
}

SelectedPointsPublisher::~SelectedPointsPublisher()
{
}

void SelectedPointsPublisher::updateTopic()
{
    nh_.param("frame_id", tf_frame_, std::string("/base_link"));
    rviz_cloud_topic_ = std::string("/rviz_selected_points");
    real_cloud_topic_ = std::string("/real_selected_points");
    subs_cloud_topic_ = std::string("/camera/depth_registered/points");
    bb_marker_topic_ = std::string("visualization_marker");

    rviz_selected_pub_ = nh_.advertise<sensor_msgs::PointCloud2>( rviz_cloud_topic_.c_str(), 1 );
    real_selected_pub_ = nh_.advertise<sensor_msgs::PointCloud2>( real_cloud_topic_.c_str(), 1 );
    partial_pc_pub_ = nh_.advertise<sensor_msgs::PointCloud2>( subs_cloud_topic_.c_str(), 1 );
    bb_marker_pub_ = nh_.advertise<visualization_msgs::Marker>(bb_marker_topic_.c_str(), 1);
    pc_subs_ =  nh_.subscribe(subs_cloud_topic_.c_str(),1,&SelectedPointsPublisher::PointCloudsCallback, this);

    ROS_INFO_STREAM_NAMED("SelectedPointsPublisher.updateTopic", "Publishing rviz selected points on topic " <<  nh_.resolveName (rviz_cloud_topic_) );//<< " with frame_id " << context_->getFixedFrame().toStdString() );
    ROS_INFO_STREAM_NAMED("SelectedPointsPublisher.updateTopic", "Publishing real selected points on topic " <<  nh_.resolveName (real_cloud_topic_) );//<< " with frame_id " << context_->getFixedFrame().toStdString() );
    ROS_INFO_STREAM_NAMED("SelectedPointsPublisher.updateTopic", "Publishing bounding box marker on topic " <<  nh_.resolveName (bb_marker_topic_) );//<< " with frame_id " << context_->getFixedFrame().toStdString() );

    current_pc_.reset(new pcl::PointCloud<pcl::PointXYZRGB>());
    accumulated_segment_pc_.reset(new pcl::PointCloud<pcl::PointXYZRGB>);

    num_acc_points_ = 0;
    num_selected_points_ = 0;
}

void SelectedPointsPublisher::PointCloudsCallback(const sensor_msgs::PointCloud2ConstPtr &pc_msg)
{
    // We only publish a message with the reception of the original pc (maybe we also do not need to copy the recei#include "rviz/selection/selection_manager.h"
#include "rviz/viewport_mouse_event.h"
#include "rviz/display_context.h"
#include "rviz/selection/forwards.h"
#include "rviz/properties/property_tree_model.h"
#include "rviz/properties/property.h"
#include "rviz/properties/vector_property.h"
#include "rviz/view_manager.h"
#include "rviz/view_controller.h"
#include "OGRE/OgreCamera.h"ved pc, because is what we published!)
    if(this->accumulated_segment_pc_->points.size() == 0)
    {
        ROS_INFO_STREAM_NAMED( "SelectedPointsPublisher::PointCloudsCallback", "Received PC");
    }
    // Convert ROS PC message into a pcl point cloud
    pcl::fromROSMsg(*pc_msg, *this->current_pc_);
    ROS_INFO("Got a new pc of length %d", this->current_pc_->size());
}

int SelectedPointsPublisher::processKeyEvent( QKeyEvent* event, rviz::RenderPanel* panel )
{
        if(event->type() == QKeyEvent::KeyPress)
        {
            if(event->key() == 'c' || event->key() == 'C')
            {
                ROS_INFO_STREAM_NAMED( "SelectedPointsPublisher::processKeyEvent", "Cleaning ALL previous selection (selected area and points).");
                rviz::SelectionManager* sel_manager = context_->getSelectionManager();
                rviz::M_Picked selection = sel_manager->getSelection();
                sel_manager->removeSelection(selection);
                visualization_msgs::Marker marker;
                // Set the frame ID and timestamp.  See the TF tutorials for information on these.
                marker.header.frame_id = context_->getFixedFrame().toStdString().c_str();
                marker.header.stamp = ros::Time::now();
                marker.ns = "basic_shapes";
                marker.id = 0;
                marker.type = visualization_msgs::Marker::CUBE;
                marker.action = visualization_msgs::Marker::DELETE;
                marker.lifetime = ros::Duration();

                selected_segment_pc_.reset(new pcl::PointCloud<pcl::PointXYZRGB>);
                accumulated_segment_pc_.reset(new pcl::PointCloud<pcl::PointXYZRGB>);
                bb_marker_pub_.publish(marker);

                // ADDED CODE TO ERASE BUFFER
                this->accumulated_pc_vec_.clear();
                this->_publishAccumulatedPoints();
            }
            else if(event->key() == 'r' || event->key() == 'R')
            {
                ROS_INFO_STREAM_NAMED( "SelectedPointsPublisher.processKeyEvent", "Reusing the LAST selected area to find a NEW bounding box.");
                this->_processSelectedAreaAndFindPoints();
            }
            else if(event->key() == 'q' || event->key() == 'Q')
            {
                this->_publishAccumulatedPoints();
            }
            else if (event->key() == 'b' || event->key() == 'B')
            {
                this->_publishAccumulatedPointsVec();
            }
            else if(event->key() == 'e' || event->key() == 'E')
            {
                ROS_INFO_STREAM_NAMED( "SelectedPointsPublisher.processKeyEvent",
                                       "Adding the points to the accumulated point cloud. Removing them from the original point cloud. Clearing the LAST selected area.");
                rviz::SelectionManager* sel_manager = context_->getSelectionManager();
                rviz::M_Picked selection = sel_manager->getSelection();
                sel_manager->removeSelection(selection);
                visualization_msgs::Marker marker;
                // Set the frame ID and timestamp.  See the TF tutorials for information on these.
                marker.header.frame_id = context_->getFixedFrame().toStdString().c_str();
                marker.header.stamp = ros::Time::now();
                marker.ns = "basic_shapes";
                marker.id = 0;
                marker.type = visualization_msgs::Marker::CUBE;
                marker.action = visualization_msgs::Marker::DELETE;
                marker.lifetime = ros::Duration();
                bb_marker_pub_.publish(marker);

                // First remove the selected point of the original point cloud so that they cannot be selected again:
                pcl::PointCloud<pcl::PointXYZRGB> temp_new_pc;
                extract_indices_filter_->setKeepOrganized(true);
                extract_indices_filter_->setNegative(true);
                temp_new_pc.header = this->current_pc_->header;
                extract_indices_filter_->filter(temp_new_pc);
                pcl::copyPointCloud(temp_new_pc, *this->current_pc_);

                sensor_msgs::PointCloud2 partial_pc_ros;
                pcl::toROSMsg(*this->current_pc_, partial_pc_ros);
                partial_pc_pub_.publish(partial_pc_ros);

                ROS_INFO("Partial point cloud with %d points extracted", temp_new_pc.size());

                // Then I copy the that were selected before in the accumulated point cloud (except if it is the first selected segment, then I copy the whole dense point cloud)
                if(this->accumulated_segment_pc_->points.size() == 0)
                {
                    pcl::copyPointCloud(*this->selected_segment_pc_, *this->accumulated_segment_pc_);
                    this->num_acc_points_ = this->num_selected_points_;
                    ROS_INFO("Accumulated point cloud initialized with %d points", this->num_acc_points_);
                }else{
                    //Both are dense organized point clouds and the points were selected_segment_pc_ has a not NaN value must be NaN in the accumulated_segment_pc_ (and viceversa)

                    try {
                        for(unsigned int idx_selected = 0; idx_selected < this->selected_segment_pc_->points.size(); idx_selected++)
                        {
                            if(pcl::isFinite(this->selected_segment_pc_->points.at(idx_selected)))
                            {
                                this->accumulated_segment_pc_->points.at(idx_selected) = this->selected_segment_pc_->points.at(idx_selected);
                            }
                        }

                        this->num_acc_points_ += this->num_selected_points_;
                        ROS_INFO("Added %d points, accumalted point count is: %d", this->num_selected_points_, this->num_acc_points_);
                    }
                    catch(const std::exception& e)
                    {
                        ROS_ERROR("Caught exception %s. Please redo selection", e.what());
                        rviz::SelectionManager* sel_manager = context_->getSelectionManager();
                        rviz::M_Picked selection = sel_manager->getSelection();
                        sel_manager->removeSelection(selection);
                        visualization_msgs::Marker marker;
                        // Set the frame ID and timestamp.  See the TF tutorials for information on these.
                        marker.header.frame_id = context_->getFixedFrame().toStdString().c_str();
                        marker.header.stamp = ros::Time::now();
                        marker.ns = "basic_shapes";
                        marker.id = 0;
                        marker.type = visualization_msgs::Marker::CUBE;
                        marker.action = visualization_msgs::Marker::DELETE;
                        marker.lifetime = ros::Duration();

                        selected_segment_pc_.reset(new pcl::PointCloud<pcl::PointXYZRGB>);
                        accumulated_segment_pc_.reset(new pcl::PointCloud<pcl::PointXYZRGB>);
                        bb_marker_pub_.publish(marker);

                        accumulated_segment_pc_.reset(new pcl::PointCloud<pcl::PointXYZRGB>);
                        this->num_acc_points_ = 0;
                    }
                }

                selected_segment_pc_.reset(new pcl::PointCloud<pcl::PointXYZRGB>);
                this->num_selected_points_ = 0;

                ROS_INFO_STREAM_NAMED("SelectedPointsPublisher._processSelectedAreaAndFindPoints",
                                      "Number of accumulated points (not published): "<< this->num_acc_points_);

                ROS_INFO_STREAM_NAMED( "SelectedPointsPublisher.processKeyEvent",
                                       "Select a new area and press '+' again to accumulate more points, or press 'y' to publish the accumulated point cloud.");
            }
        }
}

int SelectedPointsPublisher::processMouseEvent( rviz::ViewportMouseEvent& event )
{
    int flags = rviz::SelectionTool::processMouseEvent( event );

    // determine current selection mode
    if( event.alt() )
    {
        selecting_ = false;
    }
    else
    {
        if( event.leftDown() )
        {
            selecting_ = true;
        }
    }

    if( selecting_ )
    {
        if( event.leftUp() )
        {
            ROS_INFO_STREAM_NAMED( "SelectedPointsPublisher.processKeyEvent", "Using selected area to find a new bounding box and publish the points inside of it");
            this->_processSelectedAreaAndFindPoints();
        }
    }
    return flags;
}

int SelectedPointsPublisher::_reselectPreviousPoints() {
    if (selections_vec.size() == 0)
        return 1;

    rviz::M_Picked selection = selections_vec[0];
    rviz::PropertyTreeModel *model = model_vec[0];
    int num_points = model->rowCount();
    ROS_INFO_STREAM_NAMED( "SelectedPointsPublisher._processSelectedAreaAndFindPoints", "Number of points in the selected area: " << num_points);

    // Generate a ros point cloud message with the selected points in rviz
    sensor_msgs::PointCloud2 selected_points_ros;
    selected_points_ros.header.frame_id = context_->getFixedFrame().toStdString();
    selected_points_ros.height = 1;
    selected_points_ros.width = num_points;
    selected_points_ros.point_step = 3 * 4;
    selected_points_ros.row_step = num_points * selected_points_ros.point_step;
    selected_points_ros.is_dense = false;
    selected_points_ros.is_bigendian = false;

    selected_points_ros.data.resize( selected_points_ros.row_step );
    selected_points_ros.fields.resize( 3 );

    selected_points_ros.fields[0].name = "x";
    selected_points_ros.fields[0].offset = 0;
    selected_points_ros.fields[0].datatype = sensor_msgs::PointField::FLOAT32;
    selected_points_ros.fields[0].count = 1;

    selected_points_ros.fields[1].name = "y";
    selected_points_ros.fields[1].offset = 4;
    selected_points_ros.fields[1].datatype = sensor_msgs::PointField::FLOAT32;
    selected_points_ros.fields[1].count = 1;

    selected_points_ros.fields[2].name = "z";
    selected_points_ros.fields[2].offset = 8;
    selected_points_ros.fields[2].datatype = sensor_msgs::PointField::FLOAT32;
    selected_points_ros.fields[2].count = 1;

    for( int i = 0; i < num_points; i++ )
    {
        QModelIndex child_index = model->index( i, 0 );
        rviz::Property* child = model->getProp( child_index );
        rviz::VectorProperty* subchild = (rviz::VectorProperty*) child->childAt( 0 );
        Ogre::Vector3 vec = subchild->getVector();

        uint8_t* ptr = &selected_points_ros.data[0] + i * selected_points_ros.point_step;
        *(float*)ptr = vec.x;
        ptr += 4;
        *(float*)ptr = vec.y;
        ptr += 4;
        *(float*)ptr = vec.z;
        ptr += 4;
    }
    selected_points_ros.header.stamp = ros::Time::now();
    rviz_selected_pub_.publish( selected_points_ros );

    // Convert the ros point cloud message with the selected points into a pcl point cloud
    pcl::PointCloud<pcl::PointXYZ>::Ptr selected_points_pcl(new pcl::PointCloud<pcl::PointXYZ>());
    pcl::fromROSMsg(selected_points_ros, *selected_points_pcl);
    pcl::copyPointCloud(*selected_points_pcl, *this->current_pc_);

    // Generate an oriented bounding box around the selected points in RVIZ
    // Compute principal direction
    Eigen::Vector4f centroid;
    pcl::compute3DCentroid(*selected_points_pcl, centroid);
    Eigen::Matrix3f covariance;
    pcl::computeCovarianceMatrixNormalized(*selected_points_pcl, centroid, covariance);
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> eigen_solver(covariance, Eigen::ComputeEigenvectors);
    Eigen::Matrix3f eigDx = eigen_solver.eigenvectors();
    eigDx.col(2) = eigDx.col(0).cross(eigDx.col(1));

    // Move the points to the that reference frame
    Eigen::Matrix4f p2w(Eigen::Matrix4f::Identity());
    p2w.block<3,3>(0,0) = eigDx.transpose();
    p2w.block<3,1>(0,3) = -1.f * (p2w.block<3,3>(0,0) * centroid.head<3>());
    pcl::PointCloud<pcl::PointXYZ> cPoints;
    pcl::transformPointCloud(*selected_points_pcl, cPoints, p2w);

    pcl::PointXYZ min_pt, max_pt;
    pcl::getMinMax3D(cPoints, min_pt, max_pt);
    const Eigen::Vector3f mean_diag = 0.5f*(max_pt.getVector3fMap() + min_pt.getVector3fMap());

    // Final transform and bounding box size
    const Eigen::Quaternionf qfinal(eigDx);
    const Eigen::Vector3f tfinal = eigDx*mean_diag + centroid.head<3>();
    double bb_size_x = max_pt.x - min_pt.x;
    double bb_size_y = max_pt.y - min_pt.y;
    double bb_size_z = max_pt.z - min_pt.z;

    // NOTE: Use these two lines and change the following code (templates on PointXYZ instead of PointXYZRGB)
    // if your input cloud is not colored
    // Convert the point cloud from the callback into a xyz point cloud
    //pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_xyz(new pcl::PointCloud<pcl::PointXYZ>);
    //pcl::copyPointCloud(*this->current_pc_, *cloud_xyz);

    // Vectors for the size of the croping box
    Eigen::Vector4f cb_min(-bb_size_x/2.0, -bb_size_y/2.0, -bb_size_z/2.0, 1.0);
    Eigen::Vector4f cb_max(bb_size_x/2.0, bb_size_y/2.0, bb_size_z/2.0, 1.0);

    // We apply the inverse of the transformation of the bounding box to the whole point cloud to boxcrop it (then we do not move the box)
    Eigen::Affine3f transform = Eigen::Translation3f(tfinal)*qfinal;
    Eigen::Affine3f transform_inverse = transform.inverse();

    pcl::CropBox<pcl::PointXYZRGB> crop_filter;
    crop_filter.setTransform(transform_inverse);
    crop_filter.setMax(cb_max);
    crop_filter.setMin(cb_min);
    crop_filter.setKeepOrganized(true);
    crop_filter.setInputCloud(this->current_pc_);

    pcl::PointIndices::Ptr inliers( new pcl::PointIndices() );
    crop_filter.filter(inliers->indices);

    this->selected_segment_pc_.reset(new pcl::PointCloud<pcl::PointXYZRGB>);

    extract_indices_filter_.reset(new pcl::ExtractIndices<pcl::PointXYZRGB>());
    extract_indices_filter_->setIndices(inliers);
    extract_indices_filter_->setKeepOrganized(true);
    extract_indices_filter_->setInputCloud(this->current_pc_);
    this->selected_segment_pc_->header = this->current_pc_->header;
    extract_indices_filter_->filter(*this->selected_segment_pc_);

    this->num_selected_points_ = inliers->indices.size();

    ROS_INFO_STREAM_NAMED("SelectedPointsPublisher._processSelectedAreaAndFindPoints",
                          "Real number of points of the point cloud in the selected area (NOT published, NOT added): "<< this->num_selected_points_);
    ROS_INFO_STREAM_NAMED("SelectedPointsPublisher._processSelectedAreaAndFindPoints",
                          "\n\tPress '+' to add the current selection to the accumulated point cloud." << std::endl<<
                          "\tPress 'c' to clear the selection. " <<std::endl<<
                          "\tPress 'r' to recompute the bounding box with the current inlier points in the bounding box."<< std::endl<<
                          "\tSelect a different region (or an empty region) to clean this LAST selection.");


    // Publish the bounding box as a rectangular marker
    visualization_msgs::Marker marker;
    // Set the frame ID and timestamp.  See the TF tutorials for information on these.
    marker.header.frame_id = context_->getFixedFrame().toStdString().c_str();
    marker.header.stamp = ros::Time::now();
    marker.ns = "basic_shapes";
    marker.id = 0;
    marker.type = visualization_msgs::Marker::CUBE;
    marker.action = visualization_msgs::Marker::ADD;
    marker.pose.position.x = tfinal.x();
    marker.pose.position.y = tfinal.y();
    marker.pose.position.z = tfinal.z();
    marker.pose.orientation.x = qfinal.x();
    marker.pose.orientation.y = qfinal.y();
    marker.pose.orientation.z = qfinal.z();
    marker.pose.orientation.w = qfinal.w();
    marker.scale.x = max_pt.x - min_pt.x;
    marker.scale.y = max_pt.y - min_pt.y;
    marker.scale.z = max_pt.z - min_pt.z;
    marker.color.r = 0.0f;
    marker.color.g = 1.0f;
    marker.color.b = 0.0f;
    marker.color.a = 0.5;
    marker.lifetime = ros::Duration();
    bb_marker_pub_.publish(marker);

    // erase recently processed selections
    selections_vec.erase(selections_vec.begin());
    model_vec.erase(model_vec.begin());

    return 0;
}

int SelectedPointsPublisher::_processSelectedAreaAndFindPoints()
{
    rviz::SelectionManager* sel_manager = context_->getSelectionManager();
    rviz::M_Picked selection = sel_manager->getSelection();
    rviz::PropertyTreeModel *model = sel_manager->getPropertyModel();
    int num_points = model->rowCount();
    ROS_INFO_STREAM_NAMED( "SelectedPointsPublisher._processSelectedAreaAndFindPoints", "Number of points in the selected area: " << num_points);

    if (this->accumulated_pc_vec_.size() == 3)
        this->accumulated_pc_vec_.clear();

    /*
    if (selections_vec.size() == 3)
    {
        selections_vec.erase(selections_vec.begin());
        model_vec.erase(model_vec.begin());
    }
    selections_vec.push_back(selection);
    model_vec.push_back(model);
    */

    // Generate a ros point cloud message with the selected points in rviz
    sensor_msgs::PointCloud2 selected_points_ros;
    selected_points_ros.header.frame_id = context_->getFixedFrame().toStdString();
    selected_points_ros.height = 1;
    selected_points_ros.width = num_points;
    selected_points_ros.point_step = 3 * 4;
    selected_points_ros.row_step = num_points * selected_points_ros.point_step;
    selected_points_ros.is_dense = false;
    selected_points_ros.is_bigendian = false;

    selected_points_ros.data.resize( selected_points_ros.row_step );
    selected_points_ros.fields.resize( 3 );

    selected_points_ros.fields[0].name = "x";
    selected_points_ros.fields[0].offset = 0;
    selected_points_ros.fields[0].datatype = sensor_msgs::PointField::FLOAT32;
    selected_points_ros.fields[0].count = 1;

    selected_points_ros.fields[1].name = "y";
    selected_points_ros.fields[1].offset = 4;
    selected_points_ros.fields[1].datatype = sensor_msgs::PointField::FLOAT32;
    selected_points_ros.fields[1].count = 1;

    selected_points_ros.fields[2].name = "z";
    selected_points_ros.fields[2].offset = 8;
    selected_points_ros.fields[2].datatype = sensor_msgs::PointField::FLOAT32;
    selected_points_ros.fields[2].count = 1;

    for( int i = 0; i < num_points; i++ )
    {
        QModelIndex child_index = model->index( i, 0 );
        rviz::Property* child = model->getProp( child_index );
        rviz::VectorProperty* subchild = (rviz::VectorProperty*) child->childAt( 0 );
        Ogre::Vector3 vec = subchild->getVector();

        uint8_t* ptr = &selected_points_ros.data[0] + i * selected_points_ros.point_step;
        *(float*)ptr = vec.x;
        ptr += 4;https://www.youtube.com/watch?v=Nmsygz7XyP0
        *(float*)ptr = vec.y;
        ptr += 4;
        *(float*)ptr = vec.z;
        ptr += 4;
    }
    selected_points_ros.header.stamp = ros::Time::now();
    rviz_selected_pub_.publish( selected_points_ros );

    // Convert the ros point cloud message with the selected points into a pcl point cloud
    pcl::PointCloud<pcl::PointXYZ>::Ptr selected_points_pcl(new pcl::PointCloud<pcl::PointXYZ>());
    pcl::fromROSMsg(selected_points_ros, *selected_points_pcl);
    pcl::copyPointCloud(*selected_points_pcl, *this->current_pc_);

    // Generate an oriented bounding box around the selected points in RVIZ
    // Compute principal direction
    Eigen::Vector4f centroid;
    pcl::compute3DCentroid(*selected_points_pcl, centroid);
    Eigen::Matrix3f covariance;
    pcl::computeCovarianceMatrixNormalized(*selected_points_pcl, centroid, covariance);
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> eigen_solver(covariance, Eigen::ComputeEigenvectors);
    Eigen::Matrix3f eigDx = eigen_solver.eigenvectors();
    eigDx.col(2) = eigDx.col(0).cross(eigDx.col(1));

    // Move the points to the that reference frame
    Eigen::Matrix4f p2w(Eigen::Matrix4f::Identity());
    p2w.block<3,3>(0,0) = eigDx.transpose();
    p2w.block<3,1>(0,3) = -1.f * (p2w.block<3,3>(0,0) * centroid.head<3>());
    pcl::PointCloud<pcl::PointXYZ> cPoints;
    pcl::transformPointCloud(*selected_points_pcl, cPoints, p2w);

    pcl::PointXYZ min_pt, max_pt;
    pcl::getMinMax3D(cPoints, min_pt, max_pt);
    const Eigen::Vector3f mean_diag = 0.5f*(max_pt.getVector3fMap() + min_pt.getVector3fMap());

    // Final transform and bounding box size
    const Eigen::Quaternionf qfinal(eigDx);
    const Eigen::Vector3f tfinal = eigDx*mean_diag + centroid.head<3>();
    double bb_size_x = max_pt.x - min_pt.x;
    double bb_size_y = max_pt.y - min_pt.y;
    double bb_size_z = max_pt.z - min_pt.z;

    // NOTE: Use these two lines and change the following code (templates on PointXYZ instead of PointXYZRGB)
    // if your input cloud is not colored
    // Convert the point cloud from the callback into a xyz point cloud
    //pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_xyz(new pcl::PointCloud<pcl::PointXYZ>);
    //pcl::copyPointCloud(*this->current_pc_, *cloud_xyz);

    // Vectors for the size of the croping box
    Eigen::Vector4f cb_min(-bb_size_x/2.0, -bb_size_y/2.0, -bb_size_z/2.0, 1.0);
    Eigen::Vector4f cb_max(bb_size_x/2.0, bb_size_y/2.0, bb_size_z/2.0, 1.0);

    // We apply the inverse of the transformation of the bounding box to the whole point cloud to boxcrop it (then we do not move the box)
    Eigen::Affine3f transform = Eigen::Translation3f(tfinal)*qfinal;
    Eigen::Affine3f transform_inverse = transform.inverse();

    pcl::CropBox<pcl::PointXYZRGB> crop_filter;
    crop_filter.setTransform(transform_inverse);
    crop_filter.setMax(cb_max);
    crop_filter.setMin(cb_min);
    crop_filter.setKeepOrganized(true);
    crop_filter.setInputCloud(this->current_pc_);

    pcl::PointIndices::Ptr inliers( new pcl::PointIndices() );
    crop_filter.filter(inliers->indices);

    this->selected_segment_pc_.reset(new pcl::PointCloud<pcl::PointXYZRGB>);

    extract_indices_filter_.reset(new pcl::ExtractIndices<pcl::PointXYZRGB>());
    extract_indices_filter_->setIndices(inliers);
    extract_indices_filter_->setKeepOrganized(true);
    extract_indices_filter_->setInputCloud(this->current_pc_);
    this->selected_segment_pc_->header = this->current_pc_->header;
    extract_indices_filter_->filter(*this->selected_segment_pc_);

    this->num_selected_points_ = inliers->indices.size();
    ROS_INFO_STREAM_NAMED("SelectedPointsPublisher._processSelectedAreaAndFindPoints",
                          "Real number of points of the point cloud in the selected area (NOT published, NOT added): "<< this->num_selected_points_);
    ROS_INFO_STREAM_NAMED("SelectedPointsPublisher._processSelectedAreaAndFindPoints",
                          "\n\tPress '+' to add the current selection to the accumulated point cloud." << std::endl<<
                          "\tPress 'c' to clear the selection. " <<std::endl<<
                          "\tPress 'r' to recompute the bounding box with the current inlier points in the bounding box."<< std::endl<<
                          "\tSelect a different region (or an empty region) to clean this LAST selection.");


    // Publish the bounding box as a rectangular marker
    visualization_msgs::Marker marker;
    // Set the frame ID and timestamp.  See the TF tutorials for information on these.
    marker.header.frame_id = context_->getFixedFrame().toStdString().c_str();
    marker.header.stamp = ros::Time::now();
    marker.ns = "basic_shapes";
    marker.id = 0;
    marker.type = visualization_msgs::Marker::CUBE;
    marker.action = visualization_msgs::Marker::ADD;
    marker.pose.position.x = tfinal.x();
    marker.pose.position.y = tfinal.y();
    marker.pose.position.z = tfinal.z();
    marker.pose.orientation.x = qfinal.x();
    marker.pose.orientation.y = qfinal.y();
    marker.pose.orientation.z = qfinal.z();
    marker.pose.orientation.w = qfinal.w();
    marker.scale.x = max_pt.x - min_pt.x;
    marker.scale.y = max_pt.y - min_pt.y;
    marker.scale.z = max_pt.z - min_pt.z;
    marker.color.r = 0.0f;
    marker.color.g = 1.0f;
    marker.color.b = 0.0f;
    marker.color.a = 0.5;
    marker.lifetime = ros::Duration();
    bb_marker_pub_.publish(marker);

    return 0;
}

int SelectedPointsPublisher::_publishAccumulatedPointsVec()
{
    ROS_INFO_STREAM("Publishing stored pointclouds");
    if (this->accumulated_pc_vec_.size() < 3)
        return 1;
    ROS_INFO_STREAM("-------------------------------------------------------------------------------");
    for (size_t i = 0; i < this->accumulated_pc_vec_.size(); ++i)
    {
        sensor_msgs::PointCloud2 pc2 = this->accumulated_pc_vec_[i];
        ROS_INFO_STREAM("Publishing stored pointcloud number " << i << " " << pc2.height << "," <<pc2.width);
        real_selected_pub_.publish(pc2);
        ros::Duration st(0.1);
        st.sleep();
    }
    ROS_INFO_STREAM("-------------------------------------------------------------------------------");
    return 0;
}

int SelectedPointsPublisher::_publishAccumulatedPoints()
{
    ROS_INFO_STREAM_NAMED("SelectedPointsPublisher._processSelectedAreaAndFindPoints",
                          "Publishing the accumulated point cloud (" << this->num_acc_points_ << " points)");
    accumulated_segment_pc_->header = this->current_pc_->header;
    sensor_msgs::PointCloud2 accumulated_segment_ros;
    pcl::toROSMsg(*this->accumulated_segment_pc_, accumulated_segment_ros);
    real_selected_pub_.publish(accumulated_segment_ros);

    //add to accumulated vector and publish later
    this->accumulated_pc_vec_.push_back(accumulated_segment_ros);

    ROS_WARN_STREAM_NAMED("SelectedPointsPublisher._processSelectedAreaAndFindPoints",
                          "Cleaning the accumulated point cloud after publishing");
    accumulated_segment_pc_.reset(new pcl::PointCloud<pcl::PointXYZRGB>);
    this->num_acc_points_ = 0;
    return 0;
}

int SelectedPointsPublisher::_publishAccumulatedPointsNoClean()
{
    ROS_INFO_STREAM_NAMED("SelectedPointsPublisher._processSelectedAreaAndFindPoints",
                          "Publishing the accumulated point cloud (" << this->num_acc_points_ << " points)");
    accumulated_segment_pc_->header = this->current_pc_->header;
    sensor_msgs::PointCloud2 accumulated_segment_ros;
    pcl::toROSMsg(*this->accumulated_segment_pc_, accumulated_segment_ros);
    real_selected_pub_.publish(accumulated_segment_ros);

    ROS_WARN_STREAM_NAMED("SelectedPointsPublisher._processSelectedAreaAndFindPoints",
                          "Not cleaning the accumulated point cloud after publishing");
    return 0;
}

} // end namespace rviz_plugin_selected_points_topic

#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS( rviz_plugin_selected_points_publisher::SelectedPointsPublisher, rviz::Tool )
