#include "registration.h"

void Registration::setClouds(vector<SegmentedPointsContainer::SegmentedPlane> &source, vector<SegmentedPointsContainer::SegmentedPlane> &target, bool isMesh)
{
    this->source = source;
    this->target = target;
    this->targetIsMesh = isMesh;
}

mat3 Registration::findAlignment()
{
    if(target.empty() || source.empty()) return mat3::Identity();

    mat3 R = findRotation();

    return R;
}

mat3 Registration::findRotation()
{
    computeM();
    mat3 H = computeH();
    return computeR(H);
}

mat3 Registration::findTranslation()
{
    return mat3::Zero();
}

void Registration::computeM()
{
    if(target.empty() || source.empty()) return;

    M.resize(target.size(), source.size());

    #pragma omp parallel for
    for(size_t i = 0; i < target.size(); ++i)
    {
        vec3 ni = target[i].plane.getNormal();
        if(!targetIsMesh) ni *= target[i].indices_list.size();

        for(size_t j = 0; j < source.size(); ++j)
        {
            M(i, j) = squaredDistance(ni, source[j].plane.getNormal());
        }
    }
}

mat3 Registration::computeH()
{
    // Compute centroids
    //vec3 cs = computeCentroid(source, false);
    //vec3 ct = computeCentroid(target, this->targetIsMesh);

    // Compute difference sets
    //vector<diffs>

    mat3 H = mat3::Zero();

    for(int i = 0; i < source.size(); ++i)
    {
        for(int j = 0; j < target.size(); ++j)
        {
            H += M(i, j) * source[i].plane.getNormal() * target[j].plane.getNormal().transpose();
        }
    }

    return H;
}

mat3 Registration::computeR(mat3 H)
{
    Eigen::JacobiSVD<mat3> svd(H, Eigen::ComputeFullU | Eigen::ComputeFullV);
    mat3 u = svd.matrixU();
    mat3 v = svd.matrixV();
    return v * u.transpose();
}

vec3 Registration::computeCentroid(vector<SegmentedPointsContainer::SegmentedPlane> &list, bool isMesh)
{
    vec3 c = vec3::Zero();

    for(auto plane: list)
    {
        vec3 n = plane.plane.getNormal();
        if(!isMesh) n *= plane.indices_list.size();
        c += n;
    }

    c /= list.size();
    return c;
}