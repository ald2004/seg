#pragma once

#define CATCH_CONFIG_MAIN
#include "catch.hpp"



int main(int argc, char** argv)
{
	if (argc != 6) {
		fprintf(stderr, "usage: %s sigma k min input(ppm) output(ppm)\n", argv[0]);
		return 1;
	}
	float sigma = atof(argv[1]);
	float k = atof(argv[2]);
	int min_size = atoi(argv[3]);
	printf("loading input image[%s] ...\n", argv[4]);
	cv::Mat input = cv::imread(argv[4], 1);
	//cv::namedWindow("test", cv::WINDOW_NORMAL);
	//cv::imshow("test", input);
	//int key = (cv::waitKey(0) & 0xFF);
	//if (key == 'q') cv::destroyAllWindows();

	printf("processing ...\n");
	cv::Mat seg = segment_image(input, sigma, k, min_size);

	double min, max;
	cv::minMaxLoc(seg, &min, &max);

	int nb_segs = (int)max + 1;

	cv::Mat output_image = cv::Mat::zeros(seg.rows, seg.cols, CV_8UC3);

	uint* p;
	uchar* p2;

	for (int i = 0; i < seg.rows; i++) {

		p = seg.ptr<uint>(i);
		p2 = output_image.ptr<uchar>(i);

		for (int j = 0; j < seg.cols; j++) {
			cv::Scalar color = color_mapping(p[j]);
			p2[j * 3] = (uchar)color[0];
			p2[j * 3 + 1] = (uchar)color[1];
			p2[j * 3 + 2] = (uchar)color[2];
		}
	}

	//savePPM(seg, argv[5]);
	cv::imwrite(argv[5], output_image);

	std::cout << "Image written to " << argv[5] << std::endl;
	printf("got %d components\n", nb_segs);
	printf("done! uff...thats hard work.\n");
	return 0;
}
