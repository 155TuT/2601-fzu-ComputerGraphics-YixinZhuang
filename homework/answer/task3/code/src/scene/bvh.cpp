#include "bvh.h"
#include "pathtracer/render_options.h"

#include "CGL/CGL.h"
#include "triangle.h"

#include <iostream>
#include <stack>

using namespace std;

namespace CGL {
namespace SceneObjects {

BVHAccel::BVHAccel(const std::vector<Primitive *> &_primitives,
                   size_t max_leaf_size) {

  primitives = std::vector<Primitive *>(_primitives);
  root = construct_bvh(primitives.begin(), primitives.end(), max_leaf_size);
}

BVHAccel::~BVHAccel() {
  if (root)
    delete root;
  primitives.clear();
}

  BBox BVHAccel::get_bbox() const { return root->bb; }

void BVHAccel::draw(BVHNode *node, const Color &c, float alpha) const {
  if (node->isLeaf()) {
    for (auto p = node->start; p != node->end; p++) {
      (*p)->draw(c, alpha);
    }
  } else {
    draw(node->l, c, alpha);
    draw(node->r, c, alpha);
  }
}

void BVHAccel::drawOutline(BVHNode *node, const Color &c, float alpha) const {
  if (node->isLeaf()) {
    for (auto p = node->start; p != node->end; p++) {
      (*p)->drawOutline(c, alpha);
    }
  } else {
    drawOutline(node->l, c, alpha);
    drawOutline(node->r, c, alpha);
  }
}

BVHNode *BVHAccel::construct_bvh(std::vector<Primitive *>::iterator start,
                                 std::vector<Primitive *>::iterator end,
                                 size_t max_leaf_size) {
  BBox bounds, centroids;
  for (auto p=start; p!=end; ++p) {
    bounds.expand((*p)->get_bbox());
    centroids.expand((*p)->get_bbox().centroid());
  }
  BVHNode* node = new BVHNode(bounds);
  node->start=start; node->end=end;
  const size_t count=end-start;
  if (count<=std::max(size_t(1),max_leaf_size)) return node;
  int axis=0;
  if (centroids.extent.y>centroids.extent[axis]) axis=1;
  if (centroids.extent.z>centroids.extent[axis]) axis=2;
  auto middle=start+count/2;
  std::nth_element(start,middle,end,[axis](Primitive* a,Primitive* b) {
    return a->get_bbox().centroid()[axis]<b->get_bbox().centroid()[axis];
  });
  node->l=construct_bvh(start,middle,max_leaf_size);
  node->r=construct_bvh(middle,end,max_leaf_size);
  return node;
}

bool BVHAccel::has_intersection(const Ray &ray, BVHNode *node) const {
  if (render_options().brute_force && node==root) {
    for (auto p : primitives) {
      total_isects.fetch_add(1,std::memory_order_relaxed);
      if (p->has_intersection(ray)) return true;
    }
    return false;
  }

  if (!node) return false;
  double lo=ray.min_t, hi=ray.max_t;
  if (!node->bb.intersect(ray,lo,hi)) return false;
  if (node->isLeaf()) {
    for (auto p=node->start; p!=node->end; ++p) {
      total_isects.fetch_add(1,std::memory_order_relaxed);
      if ((*p)->has_intersection(ray)) return true;
    }
    return false;
  }
  return has_intersection(ray,node->l) || has_intersection(ray,node->r);
}

bool BVHAccel::intersect(const Ray &ray, Intersection *i, BVHNode *node) const {
  if (render_options().brute_force && node==root) {
    bool hit=false;
    for (auto p : primitives) {
      total_isects.fetch_add(1,std::memory_order_relaxed);
      hit=p->intersect(ray,i)||hit;
    }
    return hit;
  }

  if (!node) return false;
  double lo=ray.min_t, hi=ray.max_t;
  if (!node->bb.intersect(ray,lo,hi)) return false;
  if (node->isLeaf()) {
    bool hit=false;
    for (auto p=node->start; p!=node->end; ++p) {
      total_isects.fetch_add(1,std::memory_order_relaxed);
      hit=(*p)->intersect(ray,i)||hit;
    }
    return hit;
  }
  double left_t=ray.min_t, left_max=ray.max_t;
  double right_t=ray.min_t, right_max=ray.max_t;
  bool left=node->l->bb.intersect(ray,left_t,left_max);
  bool right=node->r->bb.intersect(ray,right_t,right_max);
  if (!left && !right) return false;
  if (!right) return intersect(ray,i,node->l);
  if (!left) return intersect(ray,i,node->r);
  BVHNode* near_node=left_t<=right_t ? node->l : node->r;
  BVHNode* far_node=left_t<=right_t ? node->r : node->l;
  bool hit=intersect(ray,i,near_node);
  return intersect(ray,i,far_node)||hit;
}

} // namespace SceneObjects
} // namespace CGL
