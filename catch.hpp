#pragma once
#include <iostream>
#include <vector>
#include <algorithm>
#include "opencv2/opencv.hpp"
using namespace std;

template <class T>
inline T square(const T& x) { return x * x; };
class Edge {
public:
	int from;
	int to;
	float weight;

	bool operator <(const Edge& e) const {
		return weight < e.weight;
	}
};

class PointSetElement {
public:
	int p;
	int size;

	PointSetElement() { }

	PointSetElement(int p_) {
		p = p_;
		size = 1;
	}
};

// An object to manage set of points, who can be fusionned
class PointSet {
public:
	PointSet(int nb_elements_) {
		nb_elements = nb_elements_;

		mapping = new PointSetElement[nb_elements];

		for (int i = 0; i < nb_elements; i++) {
			mapping[i] = PointSetElement(i);
		}
	}
	~PointSet() {
		delete[] mapping;
	}

	int nb_elements;

	// Return the main point of the point's set
	int getBasePoint(int p) {

		int base_p = p;

		while (base_p != mapping[base_p].p) {
			base_p = mapping[base_p].p;
		}

		// Save mapping for faster acces later
		mapping[p].p = base_p;

		return base_p;
	}

	// Join two sets of points, based on their main point
	void joinPoints(int p_a, int p_b) {

		// Always target smaller set, to avoid redirection in getBasePoint
		if (mapping[p_a].size < mapping[p_b].size)
			swap(p_a, p_b);

		mapping[p_b].p = p_a;
		mapping[p_a].size += mapping[p_b].size;

		nb_elements--;
	}

	// Return the set size of a set (based on the main point)
	int size(unsigned int p) { return mapping[p].size; }

private:
	PointSetElement* mapping;

};


// dissimilarity measure between pixels
static inline float diff(cv::Mat* dst, int xx1, int yy1, int xx2, int yy2) {
	int nb_channels(dst->channels());
	const float* p1 = dst->ptr<float>(yy1);
	const float* p2 = dst->ptr<float>(yy2);
	float tmp_total = 0;
	for (int channel = 0; channel < nb_channels; channel++) {
		tmp_total += pow(p1[xx1 * nb_channels + channel] - p2[xx2 * nb_channels + channel], 2);
	}return sqrt(tmp_total);;
}

cv::Mat segment_image(cv::Mat input, float sigma, float k, int  min_size) {
	int width(input.cols), height(input.rows);
	cout << width << '-' << height << '-' << input.channels() << endl;
	//vector<cv::Mat> rgbChannels(3);
	//cv::split(input, rgbChannels); //bgr
	//for (auto i : rgbChannels)cout << i.cols << '-' << i.rows << '-' << i.channels() << endl;

	//smooth each color channel  
		//Switch to float
	cv::Mat tmp, dst;
	input.convertTo(tmp, CV_32F);
	// Apply gaussian filter

	GaussianBlur(tmp, dst, cv::Size(0, 0), sigma, sigma);
	// Build graph
	Edge* edges = new Edge[width * height * 4];
	int nb_edges(0);
	for (int y(0); y < height; y++) {
		//const float* p = dst.ptr<float>(y);
		for (int x(0); x < width; x++) {
			//const float* p2 = dst.ptr<float>(x);
			if (x < width - 1) {
				edges[nb_edges].from = y * width + x;
				edges[nb_edges].to = y * width + (x + 1);
				edges[nb_edges].weight = diff(&dst, x, y, x + 1, y);
				nb_edges++;
			}

			if (y < height - 1) {
				edges[nb_edges].from = y * width + x;
				edges[nb_edges].to = (y + 1) * width + x;
				edges[nb_edges].weight = diff(&dst, x, y, x, y + 1);
				nb_edges++;
			}

			if ((x < width - 1) && (y < height - 1)) {
				edges[nb_edges].from = y * width + x;
				edges[nb_edges].to = (y + 1) * width + (x + 1);
				edges[nb_edges].weight = diff(&dst, x, y, x + 1, y + 1);
				nb_edges++;
			}

			if ((x < width - 1) && (y > 0)) {
				edges[nb_edges].from = y * width + x;
				edges[nb_edges].to = (y - 1) * width + (x + 1);
				edges[nb_edges].weight = diff(&dst, x, y, x + 1, y - 1);
				nb_edges++;
			}
		}
	}
	cout << "length of edges:" << nb_edges << "..." << endl;
	for (int i(0); i < 10; i++)cout << edges[i].from << '-' << edges[i].to << '-' << edges[i].weight << endl;

	// segment graph
	int total_points = (int)(width * height);
		// Create a set with all point (by default mapped to themselfs)
	PointSet* es = new PointSet(total_points);
		//sort edges 
	sort(edges, edges + nb_edges);
		//thresholds
	float* thresholds = new float[total_points];
	for (int i = 0; i < total_points; i++)thresholds[i] = k;
	for (int i = 0; i < nb_edges; i++) {
		int p_a = es->getBasePoint(edges[i].from);
		int p_b = es->getBasePoint(edges[i].to);
		if (p_a != p_b) {
			if (edges[i].weight <= thresholds[p_a] && edges[i].weight <= thresholds[p_b]) {
				es->joinPoints(p_a, p_b);
				p_a = es->getBasePoint(p_a);
				thresholds[p_a] = edges[i].weight + k / es->size(p_a);
				edges[i].weight = 0;
			}
		}
	}
	delete[] thresholds;

	// Remove small areas
	for (int i = 0; i < nb_edges; i++) {
		if (edges[i].weight > 0) {
			int p_a = es->getBasePoint(edges[i].from);
			int p_b = es->getBasePoint(edges[i].to);
			if (p_a != p_b && (es->size(p_a) < min_size || es->size(p_b) < min_size)) {
				es->joinPoints(p_a, p_b);

			}
		}
	}

	// Map to final output
	cv::Mat output;
	dst.copyTo(output); output.setTo(0);
	int maximum_size = (int)(output.rows * output.cols);
	int last_id = 0;
	int* mapped_id = new int[maximum_size];
	for (int i = 0; i < maximum_size; i++)mapped_id[i] = -1;
	int rows = output.rows;
	int cols = output.cols;
	if (output.isContinuous()) {
		cols *= rows;
		rows = 1;
	}
	for (int i = 0; i < rows; i++) {
		int* p = output.ptr<int>(i);
		for (int j = 0; j < cols; j++) {
			int point = es->getBasePoint(i * cols + j);
			if (mapped_id[point] == -1) {
				mapped_id[point] = last_id;
				last_id++;
			}
			p[j] = mapped_id[point];
		}
	}
	delete[] mapped_id;
	delete[] edges;
	delete es;


	return output;
}
cv::Scalar hsv_to_rgb(cv::Scalar c) {
	cv::Mat in(1, 1, CV_32FC3);
	cv::Mat out(1, 1, CV_32FC3);

	float* p = in.ptr<float>(0);

	p[0] = (float)c[0] * 360.0f;
	p[1] = (float)c[1];
	p[2] = (float)c[2];

	cvtColor(in, out, cv::COLOR_HSV2RGB);

	cv::Scalar t;

	cv::Vec3f p2 = out.at<cv::Vec3f>(0, 0);

	t[0] = (int)(p2[0] * 255);
	t[1] = (int)(p2[1] * 255);
	t[2] = (int)(p2[2] * 255);

	return t;

}
cv::Scalar color_mapping(int segment_id) {

	double base = (double)(segment_id) * 0.618033988749895 + 0.24443434;

	return hsv_to_rgb(cv::Scalar(fmod(base, 1.2), 0.95, 0.80));

}
