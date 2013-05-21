/**
 * @brief Helper functions to pick random elements from std containers
 * @file Random.h
 *
 * This file is created at Almende B.V. It is open-source software and part of the Common Hybrid Agent Platform (CHAP).
 * A toolbox with a lot of open-source tools, ranging from thread pools and TCP/IP components to control architectures
 * and learning algorithms. This software is published under the GNU Lesser General Public license (LGPL).
 *
 * It is not possible to add usage restrictions to an open-source license. Nevertheless, we personally strongly object
 * to this software being used by the military, in factory farming, for animal experimentation, or anything that
 * violates the Universal Declaration of Human Rights.
 *
 * Copyright © 2013 Anne van Rossum <anne@almende.com>
 *
 * @author  Anne C. van Rossum
 * @date    May 7, 2013
 * @project Replicator FP7
 * @company Almende B.V.
 * @case    Artificial Intelligence Framework
 */

#ifndef HOUGH_H_
#define HOUGH_H_

#include <cstring>
#include <vector>
#include <iostream>
#include <cmath>
#include <cassert>

#include <nd-array.hpp>
#include <Accumulator.h>
#include <HoughDefs.h>
#include <Random.h>

//! Use the nd-array also for the point cloud
typedef nd_array < std::vector<Point2D>,short > pointcloud;

namespace dobots {

/**
 * The Hough transform can be used to detect higher-order structures from a bunch of points, commonly called a point
 * cloud. It's first implementation used it to detect lines, we will use it to detect lines first, and planes later.
 */
template <typename P>
class Hough {
public:
	//! Default constructor expects image points in size of 640x480 and uses a Hough space of size 100x100
	Hough(): type(RANDOMIZED_HOUGH) {
		std::cout << "Use standard constructor for Hough transform" << std::endl;
		clear();
		ASize size;
		size.x = ACCUMULATOR_SIZE_X; // ACCUMULATOR_DATA_TYPE
		size.y = ACCUMULATOR_SIZE_Y;
		input_size.x = 640;
		input_size.y = 480;
		max_distance = std::sqrt(input_size.x*input_size.x + input_size.y*input_size.y);
		accumulator = new Accumulator(size);
	}

	//! Constructor with non-standard size for the Hough space
	Hough(ISize input_size, ASize hough_space_size): type(RANDOMIZED_HOUGH) {
		clear();
		this->input_size = input_size;
		max_distance = std::sqrt(input_size.x*input_size.x + input_size.y*input_size.y);
		accumulator = new Accumulator(hough_space_size);
	}

	//! Default destructor
	virtual ~Hough() {
		clear();
		if (accumulator != NULL)
			delete accumulator;
		accumulator = NULL;
	}

	//! Set the transform, it's your responsible to clear the point cloud and or reset the accumulator
	inline void setType(HoughTransformType type) { this->type = type; }

	//! Add points one by one
	void addPoint(P p) {
		points.push_back(p);
	}

	//! Add points in a bunch, points should just be two or three elements, so by value, not by reference
	void addPoints(std::vector<P> & point_cloud) {
		points.insert(points.end(), point_cloud.begin(), point_cloud.end());
	}

	//! Add points to spatial point cloud
	void addPoints(pointcloud & spatial_point_cloud) {
		spatial_points = spatial_point_cloud;
		updatePoints();
	}

	void pickRandomPatch(int &x, int &y) {
		// pick first a random index from spatial_points patch grid
		int px = spatial_points.get_dimension(0);
		int py = spatial_points.get_dimension(1);
		int size = px*py;
		int linear_r = random_value(0,size-1);
		x = linear_r % px;
		y = linear_r / px;
	}

	void updatePoints() {
		points.clear();
		std::cout << "Add points to linear array" << std::endl;
		for (int i = 0; i < spatial_points.size(); ++i) {
			addPoints(spatial_points.getf(i));
		}
		std::cout << "Points added to linear array" << std::endl;
	}

	//! This is a better method if the density differs across the cells, it requires all points to be added to one
	//! data structure. It will pick a point at random and return the given patch indices.
	void pickRandomPoint(int &x, int &y) {
		int linear_e = random_value(0,points.size()-1);
		int wx = input_size.x / spatial_points.get_dimension(0);
		int wy = input_size.y / spatial_points.get_dimension(1);

		x = points[linear_e].x / wx;
		y = points[linear_e].y / wy;
//		std::cout << "Got point " << points[linear_e].x << ',' << points[linear_e].y <<
//				" which becomes patch " << x << ',' << y << std::endl;
	}

	//! Perform the actual transform on all the points hitherto received
	void doTransform() {
		int ix, iy;
#ifdef POINTS_EQUALLY_DISTRIBUTED
		pickRandomPatch(ix,iy);
#endif
		pickRandomPoint(ix,iy);
		std::vector<P> points = spatial_points.get(ix,iy);

		if (points.size() < 3) return;
		std::vector<P> random_set; random_set.clear();
		random_set.resize(2, P());
		random_n(points.begin(), points.end(), random_set.begin(), 2);
		P pnt0 = random_set[0];
		P pnt1 = random_set[1];
		ACoordinates c = transform(pnt0, pnt1);
		Segment2D segment;
		segment.src = pnt0;
		segment.dest = pnt1;
		getAccumulator()->Increment(c, segment);
	}

	template <typename P1, typename C1>
	C1 transform(P1 pnt0, P1 pnt1) {
		assert(false); // no general implementation possible
		return pnt0;
	}

	/**
	 * Requires
	 */
	template <typename T>
	void getLineDescription(Point2D point0, Point2D point1, T & slope, T & y_intersect) {
		//reorder, such that the point with highest x comes later, this will make the slope well-defined
		Point2D pnt0, pnt1;
		if (point0.x < point1.x) {
			pnt0 = point0;
			pnt1 = point1;
		} else {
			pnt0 = point1;
			pnt1 = point0;
		}

		slope = (T)(pnt1.y - pnt0.y) / (T)(pnt1.x - pnt0.x);
		assert (slope);
		T alpha = atan(slope);
		y_intersect = pnt0.y - (T)(pnt0.x) * slope;
//		std::cout << "Line in rectangular coordinates: y=" << slope << "*x+" << y_intersect << std::endl;
	}

	/**
	 * Transform the line through two points to a (theta,r) coordinate pair. The angle theta ranges from -pi to +pi,
	 * here it is scaled so it fits the accumulator type.
	 */
	ACoordinates transform(Point2D point0, Point2D point1) {
		typedef double ftype;
		const ftype half_pi = 2*atan(1);
		ftype theta;
		ftype r;
		//reorder, such that the point with highest x comes later, this will make the slope well-defined
		Point2D pnt0, pnt1;
		if (point0.x < point1.x) {
			pnt0 = point0;
			pnt1 = point1;
		} else {
			pnt0 = point1;
			pnt1 = point0;
		}

//		std::cout << "Transform points " << pnt0.x << "," << pnt0.y << " and " <<
//				pnt1.x << "," << pnt1.y << std::endl;

		if (pnt0.y == pnt1.y) { // horizontal lines are special, or else division by zero
			theta = (pnt0.y >= 0) ? 2*half_pi : 0;
			r = pnt0.y;
		} else if (pnt0.x == pnt1.x) { // vertical lines are special too
//			std::cout << "VERTICAL LINE at x=" << pnt0.x << std::endl;
			theta = half_pi;
			r = pnt0.x;
		} else {
			ftype slope,yisect;
			getLineDescription<ftype>(pnt0,pnt1,slope,yisect);

			ftype fx = -(slope*yisect)/(slope*slope+1.0);
			ftype fy = (-1.0/slope)*fx;

			if (abs(fy) >= max_distance) {
				// too vertical to have reliable figures
				theta = half_pi;
				r = pnt0.x;
			} else {
				r = std::sqrt(fx*fx+fy*fy);
//				std::cout << "Closest point S=" << fx << "," << fy << " at dist=" << (int)r << std::endl;

				// debugging / checks
				ftype dist0 = sqrt(pnt0.x*pnt0.x+pnt0.y*pnt0.y);
				ftype dist1 = sqrt(pnt1.x*pnt1.x+pnt1.y*pnt1.y);
//				std::cout << "Point 0 is at dist=" << (int)dist0 << " and point 1 at dist=" << (int)dist1 << std::endl;
				assert (r <= dist0);
				assert (r <= dist1);

				if (!r) {
					// radial line (through origin)
					theta = atan2(pnt1.x, pnt1.y) + half_pi;
				} else {
					theta = atan2(fx, fy); // is from -PI to +PI
				}
			}
		}

		// return in Hough coordinates
		ACoordinates result;

		result.x = (r * (ftype)accumulator->getSize().x) / (max_distance*2); // r scaled with max_dist/max x
		if (result.x == accumulator->getSize().x) result.x = accumulator->getSize().x-1;

//		std::cout << "Scaled distance " << result.x << " comes from " << r << std::endl;
		result.y = ((theta) * (ftype)accumulator->getSize().y) / (4*half_pi) + accumulator->getSize().y / 2; // theta scaled with s
		if (result.y == accumulator->getSize().y) result.y = accumulator->getSize().y-1;

//		std::cout << "Scaled angle " << result.y << " comes from " << theta << std::endl;

		// for debugging
		assert (result.x < accumulator->getSize().x);
		assert (result.y < accumulator->getSize().y);
		return result;
	}

	//! Remove all points from the point cloud
	void clear() {
		//		points.clear();
	}

	//! Get the accumulator, e.g. to reset it
	inline Accumulator* getAccumulator() { return accumulator; }
private:
	//! The type of Hough transform to use
	HoughTransformType type;

	//! Accumulator
	Accumulator *accumulator;

	//! Point cloud, for now store temporary all points, and only perform transform when doTransform is called
	std::vector<P> points;

	//! Point cloud, but in a spatial structure
	pointcloud spatial_points;

	//! The input points should fall in this range
	ISize input_size;

	//! Max distance (calculated from input_size)
	int max_distance;
};

}

#endif // HOUGH_H_
