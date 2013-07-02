#include <omnimapper/omnimapper_base.h>
#include <omnimapper_ros/tf_pose_plugin.h>
#include <omnimapper/icp_pose_plugin.h>
#include <omnimapper/plane_plugin.h>
#include <omnimapper/tsdf_output_plugin.h>
#include <omnimapper/organized_feature_extraction.h>
#include <omnimapper_ros/omnimapper_visualizer_rviz.h>
#include <omnimapper_ros/OutputMapTSDF.h>
#include <omnimapper_ros/tum_data_error_plugin.h>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/io/pcd_grabber.h>
#include <pcl/io/openni_grabber.h>
#include <pcl/common/time.h>

#include <pcl/io/pcd_grabber.h>
#include <boost/filesystem.hpp>

typedef pcl::PointXYZRGBA PointT;
typedef pcl::PointCloud<PointT> Cloud;
typedef Cloud::Ptr CloudPtr;
typedef Cloud::ConstPtr CloudConstPtr;

template <typename PointT> 
class OmniMapperHandheldNode
{
  public:
    // ROS Node Handle
    ros::NodeHandle n_;
    
    // OmniMapper Instance
    omnimapper::OmniMapperBase omb_;

    // TF Pose Plugin
    omnimapper::TFPosePlugin tf_plugin_;

    // ICP Plugin
    omnimapper::ICPPoseMeasurementPlugin<PointT> icp_plugin_;

    omnimapper::ICPPoseMeasurementPlugin<PointT> edge_icp_plugin_;

    // Plane Plugin
    omnimapper::PlaneMeasurementPlugin<PointT> plane_plugin_;

    // Visualization
    omnimapper::OmniMapperVisualizerRViz<PointT> vis_plugin_;

    // TSDF Plugin
    omnimapper::TSDFOutputPlugin<PointT> tsdf_plugin_;

    // Benchmark Data Analysis
    omnimapper::TUMDataErrorPlugin error_plugin_;

    // Fake Grabber (TODO: Fix this)
    std::vector<std::string> empty_files_;
    pcl::PCDGrabber<PointT> fake_grabber_;
    // Organized Feature Extraction
    omnimapper::OrganizedFeatureExtraction<PointT> organized_feature_extraction_;

    // Subscribers
    ros::Subscriber pointcloud_sub_;

    ros::ServiceServer generate_tsdf_srv_;

    // Mapper config
    bool use_planes_;
    bool use_icp_;
    bool use_occ_edge_icp_;
    bool use_tf_;

    // ROS Params
    std::string odom_frame_name_;
    std::string base_frame_name_;
    std::string cloud_topic_name_;

    bool use_init_pose_;
    double init_x_, init_y_, init_z_;
    double init_qx_, init_qy_, init_qz_, init_qw_;

    // ICP Params
    double icp_leaf_size_;
    double icp_max_correspondence_distance_;
    double icp_trans_noise_;
    double icp_rot_noise_;

    // Occluding Edge ICP Params
    double occ_edge_trans_noise_;
    double occ_edge_rot_noise_;
    double occ_edge_score_thresh_;
    double occ_edge_max_correspondence_dist_;

    // Plane Plugin Params
    double plane_range_threshold_;
    double plane_angular_threshold_;
    double plane_range_noise_;
    double plane_angular_noise_;

    // TF Plugin Params
    double tf_trans_noise_;
    double tf_rot_noise_;
    
    // Visualization params
    bool draw_pose_array_;
    
    // TSDF plugin params
    bool use_tsdf_plugin_;

    // Error plugin params
    bool use_error_plugin_;

    // Other Flags
    bool add_pose_per_cloud_;
    

    OmniMapperHandheldNode ()
      : n_ ("~"),
        omb_ (),
        tf_plugin_ (&omb_),
        icp_plugin_ (&omb_),
        edge_icp_plugin_ (&omb_),
        plane_plugin_ (&omb_),
        vis_plugin_ (&omb_),
        tsdf_plugin_ (&omb_),
        error_plugin_ (&omb_),
        fake_grabber_ (empty_files_, 1.0, false),
        organized_feature_extraction_ (fake_grabber_)
    {
      // Load some params
      n_.param ("use_planes", use_planes_, true);
      n_.param ("use_icp", use_icp_, true);
      n_.param ("use_occ_edge_icp", use_occ_edge_icp_, true);
      n_.param ("use_tf", use_tf_, true);
      n_.param ("use_tsdf_plugin", use_tsdf_plugin_, true);
      n_.param ("use_error_plugin", use_error_plugin_, false);
      n_.param ("odom_frame_name", odom_frame_name_, std::string ("/odom"));
      n_.param ("base_frame_name", base_frame_name_, std::string ("/camera_depth_optical_frame"));
      n_.param ("cloud_topic_name", cloud_topic_name_, std::string ("/throttled_points"));
      n_.param ("icp_leaf_size", icp_leaf_size_, 0.05);
      n_.param ("icp_max_correspondence_distance", icp_max_correspondence_distance_, 0.5);
      n_.param ("icp_trans_noise", icp_trans_noise_, 0.1);
      n_.param ("icp_rot_noise", icp_rot_noise_, 0.1);
      n_.param ("occ_edge_trans_noise", occ_edge_trans_noise_, 0.1);
      n_.param ("occ_edge_rot_noise", occ_edge_rot_noise_, 0.1);
      n_.param ("occ_edge_score_thresh", occ_edge_score_thresh_, 0.1);
      n_.param ("occ_edge_max_correspondence_dist", occ_edge_max_correspondence_dist_, 0.1);
      n_.param ("plane_range_threshold", plane_range_threshold_, 0.6);
      n_.param ("plane_angular_threshold", plane_angular_threshold_, pcl::deg2rad (10.0));
      n_.param ("plane_range_noise", plane_range_noise_, 0.2);
      n_.param ("plane_angular_noise", plane_angular_noise_, 0.26);
      n_.param ("tf_trans_noise", tf_trans_noise_, 0.05);
      n_.param ("tf_rot_noise", tf_rot_noise_, pcl::deg2rad (10.0));
      n_.param ("use_init_pose", use_init_pose_, false);
      n_.param ("init_x", init_x_, 0.0);
      n_.param ("init_y", init_y_, 0.0);
      n_.param ("init_z", init_z_, 0.0);
      n_.param ("init_qx", init_qx_, 0.0);
      n_.param ("init_qy", init_qy_, 0.0);
      n_.param ("init_qz", init_qz_, 0.0);
      n_.param ("init_qw", init_qw_, 1.0);
      n_.param ("draw_pose_array", draw_pose_array_, true);
      n_.param ("add_pose_per_cloud", add_pose_per_cloud_, true);

      // Optionally specify an alternate initial pose
      if (use_init_pose_)
      {
        gtsam::Pose3 init_pose(gtsam::Rot3::quaternion (init_qw_, init_qx_, init_qy_, init_qz_), 
                               gtsam::Point3 (init_x_, init_y_, init_z_));
        omb_.setInitialPose (init_pose);
      }

      omb_.setSuppressCommitWindow (true);

      // Add the TF Pose Plugin
      tf_plugin_.setOdomFrameName (odom_frame_name_);
      tf_plugin_.setBaseFrameName (base_frame_name_);
      tf_plugin_.setRotationNoise (tf_trans_noise_);//0.1
      tf_plugin_.setTranslationNoise (tf_rot_noise_);//0.1
      if (use_tf_)
      {
        boost::shared_ptr<omnimapper::PosePlugin> tf_plugin_ptr (&tf_plugin_);
        omb_.addPosePlugin (tf_plugin_ptr);
      }

      // Set up an ICP Plugin
      icp_plugin_.setUseGICP (true);
      icp_plugin_.setOverwriteTimestamps (false);
      icp_plugin_.setAddIdentityOnFailure (false);
      icp_plugin_.setShouldDownsample (true);
      icp_plugin_.setLeafSize (icp_leaf_size_);//0.02
      icp_plugin_.setMaxCorrespondenceDistance (icp_max_correspondence_distance_);
      icp_plugin_.setScoreThreshold (0.8);
      icp_plugin_.setTransNoise (icp_trans_noise_);//10.1
      icp_plugin_.setRotNoise (icp_rot_noise_);//10.1
      icp_plugin_.setAddLoopClosures (true);
      icp_plugin_.setLoopClosureDistanceThreshold (1.0);
      icp_plugin_.setSaveFullResClouds (true);

      // Set up edge ICP plugin
      edge_icp_plugin_.setUseGICP (false);
      edge_icp_plugin_.setOverwriteTimestamps (false);
      edge_icp_plugin_.setAddIdentityOnFailure (false);
      edge_icp_plugin_.setShouldDownsample (false);
      edge_icp_plugin_.setMaxCorrespondenceDistance (occ_edge_max_correspondence_dist_);
      edge_icp_plugin_.setScoreThreshold (occ_edge_score_thresh_);
      edge_icp_plugin_.setTransNoise (occ_edge_trans_noise_);//10.1
      edge_icp_plugin_.setRotNoise (occ_edge_rot_noise_);//10.1
      edge_icp_plugin_.setAddLoopClosures (true);
      edge_icp_plugin_.setLoopClosureDistanceThreshold (0.15);
      edge_icp_plugin_.setSaveFullResClouds (false);

      // Set up the Feature Extraction
      plane_plugin_.setOverwriteTimestamps (false);
      plane_plugin_.setDisableDataAssociation (false);
      plane_plugin_.setRangeThreshold (plane_range_threshold_);
      plane_plugin_.setAngularThreshold (plane_angular_threshold_);
      //plane_plugin_.setAngularNoise (1.1);
      plane_plugin_.setAngularNoise (plane_angular_noise_);//0.26
      //plane_plugin_.setRangeNoise (2.2);
      plane_plugin_.setRangeNoise (plane_range_noise_);//0.2

      // Set up the feature extraction
      if (use_occ_edge_icp_)
      {
        boost::function<void (const CloudConstPtr&)> edge_icp_cloud_cb = boost::bind (&omnimapper::ICPPoseMeasurementPlugin<PointT>::cloudCallback, &edge_icp_plugin_, _1);
        organized_feature_extraction_.setOccludingEdgeCallback (edge_icp_cloud_cb);
      }

      if (use_planes_)
      {
        boost::function<void (std::vector<pcl::PlanarRegion<PointT>, Eigen::aligned_allocator<pcl::PlanarRegion<PointT> > >&, omnimapper::Time&)> plane_cb = boost::bind (&omnimapper::PlaneMeasurementPlugin<PointT>::planarRegionCallback, &plane_plugin_, _1, _2);
        organized_feature_extraction_.setPlanarRegionStampedCallback (plane_cb);
      }

      // Set the ICP Plugin on the visualizer
      boost::shared_ptr<omnimapper::ICPPoseMeasurementPlugin<PointT> > icp_ptr (&icp_plugin_);
      vis_plugin_.setICPPlugin (icp_ptr);
      
      // Subscribe to Point Clouds
      pointcloud_sub_ = n_.subscribe (cloud_topic_name_, 1, &OmniMapperHandheldNode::cloudCallback, this);//("/camera/depth/points", 1, &OmniMapperHandheldNode::cloudCallback, this);
      
      // Install the visualizer
      vis_plugin_.setDrawPoseArray (draw_pose_array_);
      boost::shared_ptr<omnimapper::OutputPlugin> vis_ptr (&vis_plugin_);
      omb_.addOutputPlugin (vis_ptr);

      // Set up the TSDF Plugin
      if (use_tsdf_plugin_)
      {
        tsdf_plugin_.setICPPlugin (icp_ptr);
        boost::shared_ptr<omnimapper::OutputPlugin> tsdf_ptr (&tsdf_plugin_);
        omb_.addOutputPlugin (tsdf_ptr);
      }

      // Set up the error Plugin
      if (use_error_plugin_)
      {
        boost::shared_ptr<omnimapper::OutputPlugin> error_plugin_ptr (&error_plugin_);
        omb_.addOutputPlugin (error_plugin_ptr);
      }

      generate_tsdf_srv_ = n_.advertiseService ("generate_map_tsdf", &OmniMapperHandheldNode::generateMapTSDFCallback, this);

      //OmniMapper thread
      omb_.setDebug (true);
      boost::thread omb_thread (&omnimapper::OmniMapperBase::spin, &omb_); 
      if (use_icp_)
        boost::thread icp_thread(&omnimapper::ICPPoseMeasurementPlugin<PointT>::spin, &icp_plugin_);
      if (use_occ_edge_icp_)
        boost::thread edge_icp_thread (&omnimapper::ICPPoseMeasurementPlugin<PointT>::spin, &edge_icp_plugin_);
    }

    void
    cloudCallback (const sensor_msgs::PointCloud2ConstPtr& msg)
    {
      ROS_INFO ("OmniMapperROS got a cloud.");
      //CloudPtr cloud (new Cloud ());
      //pcl::fromROSMsg (*msg, *cloud);
      pcl::PointCloud<pcl::PointXYZ>::Ptr xyz_cloud (new pcl::PointCloud<pcl::PointXYZ>());
      pcl::fromROSMsg (*msg, *xyz_cloud);
      CloudPtr cloud (new Cloud ());
      pcl::copyPointCloud (*xyz_cloud, *cloud);
      if (use_icp_)
      {
        printf ("Calling ICP Plugin!\n");
        icp_plugin_.cloudCallback (cloud);
      }
      organized_feature_extraction_.cloudCallback (cloud);

      if (add_pose_per_cloud_)
      {
        gtsam::Symbol sym;
        boost::posix_time::ptime header_time = cloud->header.stamp.toBoost ();
        omb_.getPoseSymbolAtTime (header_time, sym);
      }
    }
    
    bool
    generateMapTSDFCallback (omnimapper_ros::OutputMapTSDF::Request& req, omnimapper_ros::OutputMapTSDF::Response &res)
    {
      printf ("TSDF Service call, calling tsdf plugin\n");
      tsdf_plugin_.generateTSDF ();
      return (true);
    }

    // void
    // labelCloudCallback (const CloudConstPtr& cloud, const LabelCloudConstPtr& labels)
    // {
      
    // }
    
};

int
main (int argc, char** argv)
{
  ros::init (argc, argv, "OmniMapperROSHandheld");
  OmniMapperHandheldNode<PointT> omnimapper;
  ros::spin ();
}
