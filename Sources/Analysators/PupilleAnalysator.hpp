#pragma once
#ifdef _WIN32

#include <opencv2\opencv.hpp>
#include <vector>

class PupilleAnalysator {
public:
	// Structures result
	struct Pupille {
		cv::Rect zone;
		cv::RotatedRect shape;
		std::vector<cv::Point> contour;
		std::vector<cv::RotatedRect> leds;
		float excentricity;
		float solidity;
		bool isBlinking;
	};

	// Constructors
	PupilleAnalysator();
	~PupilleAnalysator() { };

	// Methods
	Pupille analyse(const cv::Mat& frameGray);

private:
	// Methods
	void _searchZoneEye(const cv::Mat& frameGray, cv::Rect& zone) const;

	void _initMat(const cv::Rect& rect, const cv::Size& size);
	void _gradMag(const cv::Mat &matIn, cv::Mat& matGradX, cv::Mat& matGradY, cv::Mat& matMag, cv::Mat& matWeigth) const;
	void _pupilCenterMap(const cv::Mat& scaledEyeROI, const cv::Mat& matWeights, const cv::Mat& matGradX, const cv::Mat& matGradY, const cv::Mat& matMag, cv::Mat& outSum) const;

	void _getResultMassCenter(const cv::Mat& centerMap, const double baseRatio, cv::Point& maxPos, cv::Point2f& massCenter, int& weight) const;
	void _createMask(const cv::Mat& inMat, const cv::Point& centerA, const cv::Point& centerB, const int radius, cv::Mat& outMask) const;
	void _getLeds(const cv::Mat& eyeRoi, const double baseRatio, const int radiusSearched, std::vector<cv::RotatedRect>& shapes) const;
	void _getContours(const cv::Mat& frameGray, const cv::Mat& mask, const cv::Point& center, const int radius, std::vector<cv::Point>& contours) const;

	void _extractResults(const cv::Mat& centerMap, Pupille& pupille) const;

	// Members
	const int _INV_FACTOR;
	const double _FACTOR;
	
	cv::Rect _zone;
	cv::Size _size;

	cv::Mat _eyeROI;
	cv::Mat _scaledEyeROI;
	cv::Mat _matGradX;
	cv::Mat _matGradY;
	cv::Mat _matMag;
	cv::Mat _matWeights;
	cv::Mat _centerMap;
};

#endif