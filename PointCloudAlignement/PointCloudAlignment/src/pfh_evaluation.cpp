#include "pfh_evaluation.h"

bool PFHEvaluation::isValidPlane(PointNormalKCloud::Ptr points, vector<int> &indices)
{
    Eigen::VectorXf pfh_histogram(125);
    pcl::PFHEstimation<PointNormalK, PointNormalK, pcl::PFHSignature125> pfh;
    pfh.computePointPFHSignature(*points, *points, indices, 5, pfh_histogram);

    // Test the histogram for plane
    /*
    cout << "PFHSignature125 dimensions: " << pfh_histogram.rows() << ", " << pfh_histogram.cols() << endl;
    for(int i = 0; i < pfh_histogram.rows(); ++i)
    {
        cout << i << ": " << pfh_histogram[i] << " ";
    }
    cout << endl;
    */

    return pfh_histogram[62] > PLANE_TRESHOLD;
}

pcl::PointCloud<pcl::PFHSignature125> PFHEvaluation::computePFHSignatures(vector<SegmentedPointsContainer::SegmentedPlane> &l_planes)
{
    // First, build cloud of points+normals of every planes' centers and normals.
    PointNormalCloud::Ptr cloud(new PointNormalCloud);
    for(auto plane: l_planes)
    {
        vec3 c(plane.plane.getCenter());
        vec3 n(plane.plane.getNormalizedN());

        PointNormal pn;
        pn.x = c.x(); pn.y = c.y(); pn.z = c.z();
        pn.normal_x = n.x(); pn.normal_y = n.y(); pn.normal_z = n.z();
        cloud->push_back(pn);
    }

    // Fill kdTree to optimise neigboring search
    pcl::search::KdTree<PointNormal>::Ptr p_kdTree(new pcl::search::KdTree<PointNormal>());

    // Output cloud
    pcl::PointCloud<pcl::PFHSignature125> pfh_cloud;

    // Compute the signatures
    pcl::PFHEstimation<PointNormal, PointNormal, pcl::PFHSignature125> pfh;
    pfh.setInputCloud(cloud);
    pfh.setInputNormals(cloud);
    pfh.setSearchMethod(p_kdTree);
    pfh.setKSearch(CENTER_KNN);
    pfh.compute(pfh_cloud);

    return pfh_cloud;
}

size_t PFHEvaluation::getMinTarget(size_t i, pcl::PointCloud<pcl::PFHSignature125> source_signs, pcl::PointCloud<pcl::PFHSignature125> target_signs, vector<size_t> &ignore_list)
{
    size_t j = 0;
    float min_error = numeric_limits<float>::infinity();

    for(size_t it = 0; it < target_signs.points.size(); ++it)
    {
        if(find(ignore_list.begin(), ignore_list.end(), it) == ignore_list.end())
        {
            float curr_error = 0;

            for(int bin_index = 0; bin_index < target_signs.points[it].descriptorSize(); ++bin_index)
            {
                curr_error += abs(source_signs.points[i].histogram[bin_index] - target_signs.points[it].histogram[bin_index]);
            }

            if(curr_error < min_error)
            {
                min_error = curr_error;
                j = it;
            }
            else if (curr_error == min_error) {
                cout << "SAME ERROR VALUE -> NEED TO CHANGE THE WAY M IS COMPUTED" << endl;
            }
        }
    }

    return j;
}

/*
pcl::PFHSignature125 PFHEvaluation::computePFHSignature(PointNormalCloud::Ptr p_cloud, KdTreeFlann::Ptr p_kdTree, SegmentedPointsContainer::SegmentedPlane &plane)
{
    // Find the neighborhood of current plane center.
    PointNormal p;
    vector<int> indices;
    vector<float> dists;
    p_kdTree->nearestKSearch(plane.plane.getPointNormal(), CENTER_KNN, indices, dists);

    // Compute the signature
    pcl::PFHEstimation<PointNormal, PointNormal, pcl::PFHSignature125> pfh;
    Eigen::VectorXf pfh_histogram(125);
    pfh.computePointPFHSignature(*p_cloud, *p_cloud, indices, 5, pfh_histogram);

    pcl::PFHSignature125 sign;

}*/
