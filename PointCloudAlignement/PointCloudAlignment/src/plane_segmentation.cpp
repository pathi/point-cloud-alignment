#include "plane_segmentation.h"

#include <algorithm>

//#include <fstream>

int PlaneSegmentation::init(string cloud_file)
{
    p_cloud = PointNormalKCloud::Ptr(new PointNormalKCloud);

    int r = pcl::io::loadPCDFile(cloud_file, *p_cloud);

    if(r == -1 && pcl::io::loadPLYFile(cloud_file, *p_cloud) == -1)
    {
        PCL_ERROR("Could not read given file\n");
        return EXIT_FAILURE;
    }

    cout << "Pointcloud containing " << p_cloud->points.size() << " points loaded." << endl;

    p_excluded_indices = PointIndices::Ptr(new PointIndices);
    p_indices = PointIndices::Ptr(new PointIndices);
    p_indices->indices.resize(p_cloud->points.size());

    #pragma omp parallel for
    for(int i = 0; i < p_cloud->points.size(); ++i)
    {
        p_indices->indices[i] = i;
    }

    cout << "PointIndices is filled" << endl;

    // Fill kdtree search strucuture
    p_kdtree = KdTreeFlannK::Ptr(new KdTreeFlannK);
    p_kdtree->setInputCloud(p_cloud);

    cout << "Starting to compute normal cloud..." << endl;

    NormalComputation nc;
    nc.computeNormalCloud(p_cloud, p_kdtree);

    cout << "Normal computation successfully ended." << endl;

    // Initialize remaining variables
    safety_distance = 0;

    //TODO: initialize segmented_points_container

    is_ready = true;
    return EXIT_SUCCESS;
}

void PlaneSegmentation::setViewerUpdateCallback(function<void(PointNormalKCloud::Ptr)> callable)
{
    display_update_callable = callable;
}

bool PlaneSegmentation::isReady()
{
    return this->is_ready;
}

void PlaneSegmentation::start_pause()
{
    if(!is_ready) {
        cout << "Can't start algorithm, not initialized properly" << endl;
        return;
    }

    is_started = !is_started;
}

void PlaneSegmentation::runMainLoop()
{
    int index;
    while(dont_quit)
    {
        while(is_started && (index = getRegionGrowingStartLocation()) != -1)
        {
            // Start of a new plane segmentation
            // -> reinitialise variables
            current_run.setupNextPlane(p_cloud->points[index]);

            segmentPlane();
        }
    }
}

void PlaneSegmentation::stop()
{
    is_started = false;
    dont_quit = false;
}

void PlaneSegmentation::stop_current_plane_segmentation()
{
    is_started = false;
}

bool PlaneSegmentation::initRegionGrowth()
{
    // Get first neighborhood
    vector<float> sqr_distances(current_run.root_p.k);
    p_kdtree->nearestKSearch(current_run.root_p, current_run.root_p.k,
                             current_run.p_nghbrs_indices->indices,
                             sqr_distances);

    // Compute mean of min distances in the neighborhood
    current_run.max_search_distance = 2.0f * getMeanOfMinDistances();

    float dist_to_kth = std::sqrt(sqr_distances[current_run.root_p.k]);

    // At 10th iteration, we ensure that we are not trying to
    // segment an already treated zone.
    if(current_run.plane_nb > 10 &&
            (dist_to_kth > 3 * safety_distance / (float)current_run.plane_nb))
    {
        // This means that the algo is looking an area
        // that has probably been already processed and is
        // trying to compute the plane of the remaining points
        // that have not been accepted in the plane previously.

        // Thus, we add the points to exclusion list since we don't want to
        // consider them again.

        //TODO: Add to exclusion list

        return false;
    }

    // Update safety distance.
    safety_distance += dist_to_kth;

    return true;
}

void PlaneSegmentation::performRegionGrowth()
{
    while(is_started)
    {
        cout << "Segmenting plane " << current_run.plane_nb << ": iteration " << current_run.iteration << endl;

        //TODO: Check for termination conditions
        if(segmented_points_container.getNbOfSegmentedPoints() > 0.8 * p_cloud->size())
        {
            //Stop, majority of points have been segmented -> Success
            stop();
            return;
        }

        current_run.prev_size = current_run.p_nghbrs_indices->indices.size();

        //TODO: If first 3 iterations, check area for valid plane
        if(current_run.iteration == PHASE1_ITERATIONS)
        {
            if(!PFHEvaluation::isValidPlane(p_cloud,
                                            current_run.p_nghbrs_indices->indices))
            {
                //TODO: Add to exclusion

                stop_current_plane_segmentation();
                return;
            }
            else
            {
                // We are on a plane -> increase search radius to speed things up
                current_run.max_search_distance *= 2.0f;
                current_run.epsilon *= 2.0f;
            }
        }

        //TODO: Check if neighborhood has shrinked
        if(current_run.p_nghbrs_indices->indices.size() < 3)
        {
            stop_current_plane_segmentation();
            return;
        }

        //TODO: Compute current plane
        if(current_run.p_nghbrs_indices->indices.size() < MIN_STABLE_SIZE)
        {
            Plane curr_plane;
            boost::shared_ptr<vector<int>> ptr_nbhrs_indices(new vector<int>(current_run.p_nghbrs_indices->indices));
            Plane::estimatePlane(p_cloud, ptr_nbhrs_indices, curr_plane);
            current_run.plane = curr_plane;

            //TODO: Update normal and epsilon
            current_run.n = curr_plane.getNormal();
            current_run.epsilon = curr_plane.getStdDevWith(p_cloud, current_run.p_nghbrs_indices);
        }

        //TODO: Find new candidates

        //TODO: Test them with current plane

        //TODO: Add good candidate to neighborhood

        //TODO: Check for region growth

        current_run.iteration++;
    }
}

void PlaneSegmentation::segmentPlane()
{
    cout << "Thread " << omp_get_thread_num() << " starting segmentation." << endl;

    if(initRegionGrowth())
    {
        performRegionGrowth();
    }

    is_started = false;
}

float PlaneSegmentation::getMeanOfMinDistances()
{
    const int K(2);
    float acc(0);

    #pragma omp parallel for shared(acc)
    for(int i = 0; i < current_run.p_nghbrs_indices->indices.size(); ++i)
    {
        int p_id = current_run.p_nghbrs_indices->indices[i];
        vector<int> indices(K);
        vector<float> sqrd_distances(K);
        p_kdtree->nearestKSearch(p_cloud->points[p_id], K, indices, sqrd_distances);
        float dist = std::sqrt(sqrd_distances[1]);

        #pragma omp critical
        acc += dist;
    }

    return acc / current_run.p_nghbrs_indices->indices.size();
}

int PlaneSegmentation::getRegionGrowingStartLocation()
{
    if(p_indices->indices.size() == 0) return -1;

    // Copy indices
    vector<int> tmp_indices(p_indices->indices);
    sort(tmp_indices.begin(), tmp_indices.end(), [this](const int &lhs, const int &rhs){
        return this->p_cloud->points[lhs].curvature < this->p_cloud->points[rhs].curvature;
    });

    /*
    ofstream myfile;
    myfile.open ("/home/loris/Documents/EPFL/Master/master-project-2019/Data/Plots/curvature.txt");
    int j = 1;
    for_each(tmp_indices.begin(), tmp_indices.end(), [this, &myfile, &j](const int &i){
        myfile << j << ";" << this->p_cloud->points[i].curvature << endl;
        ++j;
    });
    myfile.close();
    */

    return tmp_indices.at(0);
}
