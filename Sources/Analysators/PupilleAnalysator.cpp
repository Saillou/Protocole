#include "PupilleAnalysator.hpp"

#include <chrono>

using namespace std::chrono;

// --- Lil' functions ---
static float _computeExcentricity2(const cv::RotatedRect& ellipse) {
	if (ellipse.size.height == 0)
		return INFINITY;

	float a = ellipse.size.height / 2;
	float b = ellipse.size.width / 2;
	return 1.0f - (b*b) / (a*a);
}
static float _computeSolidy(const std::vector<cv::Point>& contours) {
	if (contours.size() < 2)
		return 1.0f;

	std::vector<cv::Point> hull;
	cv::convexHull(contours, hull);

	double area = cv::contourArea(contours);
	double hullArea = cv::contourArea(hull);

	if (hullArea < 1e-7)
		return area < 1e-7 ? 1.0f : INFINITY;

	return float(area / hullArea);
}

// --- Lil' operators ---
cv::Size2f operator * (float a, cv::Size2f s) {
	return cv::Size2f(a*s.width, a*s.height);
}
// ---------------------

// Constructors
PupilleAnalysator::PupilleAnalysator() :
	_INV_FACTOR(6),
	_FACTOR(1.0 / _INV_FACTOR),
	_zone(0, 0, 0, 0),
	_size(0, 0)
{

}

// Methods
PupilleAnalysator::Pupille PupilleAnalysator::analyse(const cv::Mat& frameGray) {
	Pupille pupille;

	// - Search region of interest
	_searchZoneEye(frameGray, pupille.zone);

	// - Init input
	_eyeROI = frameGray(pupille.zone).clone();

	cv::resize(_eyeROI, _scaledEyeROI, cv::Size(), _FACTOR, _FACTOR);
	_initMat(pupille.zone, _scaledEyeROI.size());

	// - Compute gradients
	_gradMag(_scaledEyeROI, _matGradX, _matGradY, _matMag, _matWeights);

	// - Approximate center position
	_pupilCenterMap(_scaledEyeROI, _matWeights, _matGradX, _matGradY, _matMag, _centerMap);

	// - Compute informations
	_extractResults(_centerMap, pupille);

	return pupille;
}

// ---- Private ---
void PupilleAnalysator::_searchZoneEye(const cv::Mat& frameGray, cv::Rect& zone) const {
	// Initialization of limits and results
	int nCols = frameGray.cols;
	int nRows = frameGray.rows;
	int nTots = nCols * nRows;

	// Height is fixed
	int heightZone = frameGray.rows / 2; // equal to y1 - y0.

	int x0 = 0;
	int x1 = nCols - 1;
	int y0 = 0;
	int y1 = heightZone;

	// ---------------------------- Value on x -----------------------------
	// Discard value too low (/2 of the max) on a side of the row selected
	const unsigned char* midRow = frameGray.data;
	const int quartCols = nCols / 4; // Limits of search

	for (int rowI = 0; rowI < nRows - 1; rowI += 4) {
		// Intensity of row -> find max value
		int maxMidValue = 0;
		for (int iCols = 0; iCols < quartCols; iCols++) {
			const unsigned char first = midRow[iCols];
			const unsigned char last = midRow[nCols - iCols - 1];

			if (last > maxMidValue)
				maxMidValue = last;

			if (first > maxMidValue)
				maxMidValue = first;
		}

		// Search first position where the limit is reached
		const int limitValue = maxMidValue / 2;

		bool c0 = true; // Continue incremente  x0
		bool c1 = true; // .. ..				x1

		int x0_possible = 0;
		int x1_possible = nCols - 1;

		for (int iCols = 0; iCols < quartCols; iCols++) {
			if (c0) {
				if (midRow[x0_possible] > limitValue/* || x0 > x0_possible*/)
					c0 = false;
				x0_possible++;
			}

			if (c1) {
				if (midRow[x1_possible] > limitValue /*|| x1 < x1_possible*/)
					c1 = false;
				x1_possible--;
			}

			if (!c0 && !c1)
				break;
		}

		// Update x0 and x1, (min/max)
		if (x0 < x0_possible)
			x0 = x0_possible;

		if (x1 > x1_possible)
			x1 = x1_possible;

		// Incremente row
		midRow = midRow + frameGray.cols;
	}

	// --------------------------- Value on y ---------------------------	
	// Search best sandwiches (max-min-max) and take the weighted (sandwich quality) position of the its min.
	const int step = 16;
	const size_t xIterations = (size_t)((x1 - x0) / step);
	const int incrementeRow = step * nCols;

	const unsigned char* rowTrunc_a = frameGray.data; // first max
	const unsigned char* rowTrunc_b = frameGray.data + nTots - incrementeRow; // last max
	const unsigned char* rowTrunc_c = nullptr; // Sandwich (min)

	// Results 
	int massCenterSandwich = 0;

	// Temporary memory
	int totalWeight = 0;
	std::vector<int> lowerSandwichPos(xIterations, 0);
	std::vector<int> upperSandwichPos(xIterations, 0);
	std::vector<int> sandwichPos(xIterations, 0);

	std::vector<unsigned char> upperSandwichVal(xIterations, 0);
	std::vector<unsigned char> lowerSandwichVal(xIterations, 0);
	std::vector<unsigned char> sandwichVal(xIterations, 0);

	for (int iRow = 0; iRow < nRows / 2; iRow += step) {
		for (int x = x0, ix = 0; ix < xIterations; x += step, ix++) {
			bool searchSandwich = false;

			if (upperSandwichVal[ix] < rowTrunc_a[x]) {
				upperSandwichVal[ix] = rowTrunc_a[x];
				upperSandwichPos[ix] = iRow;

				searchSandwich = true;
			}
			if (lowerSandwichVal[ix] < rowTrunc_b[x]) {
				lowerSandwichVal[ix] = rowTrunc_b[x];
				lowerSandwichPos[ix] = nRows - iRow - 1;

				searchSandwich = true;
			}

			if (!searchSandwich)
				continue;

			// Search inside
			unsigned char minVal = 255;
			int potentialPos = 0;

			rowTrunc_c = rowTrunc_a + x;

			for (int y = upperSandwichPos[ix]; y < lowerSandwichPos[ix]; y += step) {
				// Only the first value is used
				if (minVal > rowTrunc_c[0]) {
					minVal = rowTrunc_c[0];
					potentialPos = y;
				}

				// Incremente
				rowTrunc_c = rowTrunc_c + incrementeRow;
			}

			// Compute sandwich
			const unsigned char aip = upperSandwichVal[ix];
			const unsigned char ain = lowerSandwichVal[ix];
			const unsigned char ai = minVal;

			if (aip < ai || ain < ai)
				continue;

			unsigned char malus = std::abs(aip - ain);   // too much difference between the 2 max, range [0-255]
			unsigned char bonus = (aip + ain) / 2 - ai; // difference bettwen inside and outside, range [0-255]

			if (malus > bonus) // result < 0, obviously not the best sandwich
				continue;

			unsigned char sandwich = bonus - malus; // range [0-255]
			if (sandwich > sandwichVal[ix]) {
				sandwichVal[ix] = sandwich;
				sandwichPos[ix] = potentialPos;
			}
		}

		rowTrunc_a = rowTrunc_a + incrementeRow;
		rowTrunc_b = rowTrunc_b - incrementeRow;
	}

	for (size_t ix = 0; ix < xIterations; ix++) {
		massCenterSandwich += sandwichPos[ix] * sandwichVal[ix];
		totalWeight += sandwichVal[ix];
	}
	massCenterSandwich = massCenterSandwich / (totalWeight > 0 ? totalWeight : 1);

	// This mass center is the center of the zone
	y0 = massCenterSandwich - heightZone / 2;
	y1 = massCenterSandwich + heightZone / 2;

	// Limits
	if (y0 < 0) {
		y1 = y1 - y0;
		y0 = 0;
	}
	else if (y1 > nRows - 1) {
		y0 = y0 - (y1 - nRows + 1);
		y1 = nRows - 1;
	}



	// Finally 
	zone = cv::Rect(x0, y0, x1 - x0, heightZone);

	// Round : factor applied shouln't cut pixels
	int pixW = zone.width % _INV_FACTOR;
	int pixH = zone.height % _INV_FACTOR;

	zone.width -= pixW;
	zone.height -= pixH;
}

void PupilleAnalysator::_initMat(const cv::Rect& newZone, const cv::Size& newSize) {
	// Size
	int nbRows = newSize.height;
	int nbCols = newSize.width;

	_zone = newZone;
	if (_size != newSize) {
		_size = newSize;

		_matGradX = cv::Mat::zeros(nbRows, nbCols, CV_64F);
		_matGradY = cv::Mat::zeros(nbRows, nbCols, CV_64F);
		_matMag = cv::Mat::zeros(nbRows, nbCols, CV_64F);
		_matWeights = cv::Mat::zeros(nbRows, nbCols, CV_64F);
	}

	_centerMap = cv::Mat::zeros(nbRows, nbCols, CV_64F);
}
void PupilleAnalysator::_gradMag(const cv::Mat &matIn, cv::Mat& matGx, cv::Mat& matGy, cv::Mat& matMg, cv::Mat& matW) const {
	// Offset to add for incrementing a row
	int incrementRow = matIn.cols;

	// Init ptr to frames' rows
	unsigned char* rowInputA = matIn.data + 0;
	unsigned char* rowInputB = rowInputA + incrementRow;

	double* rowOutputGx = (double*)matGx.data + incrementRow;
	double* rowOutputGy = (double*)matGy.data + incrementRow;
	double* rowOutputMg = (double*)matMg.data + incrementRow;
	double* rowOutputWeigth = (double*)matW.data + incrementRow;

	// Cells inputs
	unsigned char inputs[9] = { 0 };
	enum NAME_INPUT { // 1st char : row | 2nd char : column
		AA, AB, AC,
		BA, BB, BC,
		CA, CB, CC,
	};

	// Iterate through frame
	for (int y = 1; y < matIn.rows - 1; y++) {
		// Fetch new input rows
		unsigned char* rowInputC = rowInputB + incrementRow;

		// Init inputs cols
		//inputs[AA] = rowInputA[0]; // Not used
		inputs[AB] = rowInputA[1];

		inputs[BA] = rowInputB[0];
		inputs[BB] = rowInputB[1];

		//inputs[CA] = rowInputC[0]; // Not used
		inputs[CB] = rowInputC[1];

		for (int x = 1; x < matIn.cols - 1; x++) {
			// Fetch new columns
			inputs[AC] = rowInputA[x + 1];
			inputs[BC] = rowInputB[x + 1];
			inputs[CC] = rowInputC[x + 1];

			// Compute gradient and magnitude
			double gradX = (inputs[BC] - inputs[BA]) / 2.0;					// {min = 0-255, min = 255-0}/0.5 => [-127.5; 127.5]
			double gradY = (inputs[CB] - inputs[AB]) / 2.0;					// ..
			double magnitude = std::sqrt(gradX * gradX + gradY * gradY);	// {min = 0^2, max = 127.5^2}^0.5 => [0; 127.5]

			rowOutputMg[x] = magnitude;
			rowOutputGx[x] = gradX / (magnitude + 0.001);		// Normalize
			rowOutputGy[x] = gradY / (magnitude + 0.001);		// ..
			rowOutputWeigth[x] = (256 - inputs[BB]) * 10;		// Inverse

			// Shift cols
			//inputs[AA] = inputs[AB]; // If used
			inputs[AB] = inputs[AC];

			inputs[BA] = inputs[BB];
			inputs[BB] = inputs[BC];

			//inputs[CA] = inputs[CB]; // If used
			inputs[CB] = inputs[CC];
		}

		// Incremente all output rows
		rowOutputGx = rowOutputGx + incrementRow;
		rowOutputGy = rowOutputGy + incrementRow;
		rowOutputMg = rowOutputMg + incrementRow;
		rowOutputWeigth = rowOutputWeigth + incrementRow;

		// Shift rows
		rowInputA = rowInputB;
		rowInputB = rowInputC;
	}
}
void PupilleAnalysator::_pupilCenterMap(const cv::Mat& matIn, const cv::Mat& matW, const cv::Mat& matGx, const cv::Mat& matGy, const cv::Mat& matMg, cv::Mat& matOut) const {
	// Define thresh to begin computing (not too small nor too big)
	double meanMag = 2.1*cv::mean(matMg)[0];

	int incrementRow = matIn.cols;

	const double *rowInputGx = (double*)matGx.data;
	const double *rowInputGy = (double*)matGy.data;
	const double *rowInputMg = (double*)matMg.data;

	const int limitSearched = 100 / _INV_FACTOR;

	for (int y = 1; y < matW.rows; y++) {
		for (int x = 1; x < matW.cols - 1; x++) {
			double gX = rowInputGx[x];
			double gY = rowInputGy[x];

			if (rowInputMg[x] < meanMag)
				continue;

			// -- begin test circle formula
			double *rowOutput = (double*)matOut.data;
			const double *rowInputW = (double*)matW.data;

			for (int cy = 1; cy < matW.rows; cy++) {
				for (int cx = 1; cx < matW.cols - 1; cx++) {
					if (x == cx && y == cy)
						continue;

					// create a vector from the possible center to the gradient origin
					int dx = x - cx;
					int dy = y - cy;

					if (dy < -limitSearched || dy > limitSearched)
						break;

					if (dx < -limitSearched || dx > limitSearched)
						continue;

					// normalize dot product
					double dotProduct = (dx * gX + dy * gY);
					if (dotProduct <= 0)
						continue;

					rowOutput[cx] += dotProduct * dotProduct * rowInputW[cx] / ((dx * dx) + (dy * dy));
				}

				// Incremente rows
				rowOutput = rowOutput + incrementRow;
				rowInputW = rowInputW + incrementRow;
			}
			// -- end test circle formula

		}

		// Incremente rows
		rowInputGx = rowInputGx + incrementRow;
		rowInputGy = rowInputGy + incrementRow;
		rowInputMg = rowInputMg + incrementRow;
	}
}

void PupilleAnalysator::_getResultMassCenter(const cv::Mat& centerMap, const double baseRatio, cv::Point& maxPos, cv::Point2f& massCenter, int& weight) const {
	// Find threshold for the algorithm result
	double maxVal;
	cv::minMaxLoc(centerMap, nullptr, &maxVal, nullptr, &maxPos);
	double threshVal = baseRatio * maxVal;

	// Compute mass center and radius around the 'most' presumed center
	cv::Mat frameMemoFlood	= cv::Mat::zeros(centerMap.rows, centerMap.cols, CV_8UC1);
	weight					= 0;
	massCenter				= cv::Point2f(0.0, 0.0);

	std::vector<int> pointToTest{ maxPos.x + maxPos.y*frameMemoFlood.cols };

	for (size_t posListTest = 0; posListTest < pointToTest.size(); posListTest++) {
		int point = pointToTest[posListTest];

		if (frameMemoFlood.data[point])
			continue;

		double ptrVal = *((double*)centerMap.data + point);

		if (ptrVal > threshVal) {
			// Get coordinates
			int x = point % frameMemoFlood.cols;
			int y = point / frameMemoFlood.cols;

			// Accumulate mass center
			massCenter.x = massCenter.x + x;
			massCenter.y = massCenter.y + y;
			weight = weight + 1;

			frameMemoFlood.data[point] = 255;

			// Propagate X 
			if (x > 1) {
				int pointLeft = point - 1;
				if (!frameMemoFlood.data[pointLeft])
					pointToTest.push_back(pointLeft);
			}
			if (x < frameMemoFlood.cols - 1) {
				int pointRight = point + 1;
				if (!frameMemoFlood.data[pointRight])
					pointToTest.push_back(pointRight);
			}

			// Propagate Y
			if (y > 1) {
				int pointTop = point - centerMap.cols;
				if (!frameMemoFlood.data[pointTop])
					pointToTest.push_back(pointTop);
			}
			if (y < frameMemoFlood.rows - 1) {
				int pointBot = point + centerMap.cols;
				if (!frameMemoFlood.data[pointBot])
					pointToTest.push_back(pointBot);
			}
		}
		else {
			frameMemoFlood.data[point] = 127;
		}
	}

	// Average
	if (weight > 0)
		massCenter = massCenter / weight;
}
void PupilleAnalysator::_createMask(const cv::Mat& inMat, const cv::Point& centerA, const cv::Point& centerB, const int radius, cv::Mat& outMask) const {
	outMask = cv::Mat::zeros(_zone.height, _zone.width, CV_8UC1);

	// Center of [A;B]
	cv::Point gAB = (centerA + centerB) / 2;
	
	// Distance from center
	int dAG = (int)std::sqrt((gAB - centerA).dot(gAB - centerA));
	int dBG = (int)std::sqrt((gAB - centerB).dot(gAB - centerB));

	// Zone (Min-Circle) enclosing circles from A and B of radius r
	cv::Size2f zoneSize = 2.f * (std::max(dAG, dBG) + radius)*cv::Size2f(1.3f, 1.1f);

	// Define rectangle where to find the mass center
	cv::Rect zoneComputeMask = cv::Rect(gAB - cv::Point((int)zoneSize.width, (int)zoneSize.height) / 2, zoneSize);
	if (zoneComputeMask.x < 0) zoneComputeMask.x = 0;
	if (zoneComputeMask.y < 0) zoneComputeMask.y = 0;
	if (zoneComputeMask.x + zoneComputeMask.width > _eyeROI.cols - 1)  zoneComputeMask.width = _eyeROI.cols - zoneComputeMask.x - 1;
	if (zoneComputeMask.y + zoneComputeMask.height > _eyeROI.rows - 1) zoneComputeMask.height = _eyeROI.rows - zoneComputeMask.y - 1;
	
	// Nonetheless, outside, not good.
	if (zoneComputeMask.width < 0 || zoneComputeMask.height < 0)
		return;

	// Search black hole mass center
	cv::Mat test = _eyeROI(zoneComputeMask).clone();
	unsigned char mean = (unsigned char)cv::mean(test)[0];

	cv::Point2f massCenterZone(0, 0);
	float weightTotal = 0.0f + 1e-7f; // don't divide by 0

	for (int y = 0; y < test.rows; y++) {
		for (int x = 0; x < test.cols; x++) {
			unsigned char val = test.data[x + y * test.cols];
			if (val > mean)
				continue;

			float w = (255 - val) / 255.0f; // Black searched so inverse.
			massCenterZone.x += x * w;
			massCenterZone.y += y * w;
			weightTotal += w;
		}
	}
	massCenterZone = massCenterZone / weightTotal + cv::Point2f(zoneComputeMask.tl());

	// Mask as an ellipse
	cv::ellipse(outMask, cv::RotatedRect(massCenterZone, zoneSize, 0.0), cv::Scalar::all(255), -1);
}

void PupilleAnalysator::_getLeds(const cv::Mat& eyeRoi, const double baseRatio, const int radiusSearched, std::vector<cv::RotatedRect>& shapes) const {
	// Min pos (at max intensity) - probably LED
	cv::Point possibleLedCenter;
	double maxVal;
	cv::minMaxLoc(eyeRoi, nullptr, &maxVal, nullptr, &possibleLedCenter);

	// Init result
	shapes.clear();

	if (maxVal < 150) // Probably nothing
		return;

	// Thresh
	cv::Mat threshedLed;
	cv::threshold(eyeRoi, threshedLed, maxVal*baseRatio, 255, cv::THRESH_BINARY);

	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(threshedLed, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

	for (auto contour : contours) {
		if (contour.size() < 6)
			continue;
		cv::RotatedRect possibleLed = cv::fitEllipse(contour);

		if (_computeExcentricity2(possibleLed) < 0.75)
			shapes.push_back(possibleLed);
	}
}
void PupilleAnalysator::_getContours(const cv::Mat& fGray, const cv::Mat& mask, const cv::Point& center, const int radius, std::vector<cv::Point>& contours) const {	
	contours.clear();
	
	// Polar projection
	const double MAX_THETA = 6.28;
	const double INC_THETA = MAX_THETA / 64.0;

	const int maxRho = 3 * radius;
	const int vStrat = 255 / 3;
	bool useMask	 = !mask.empty();

	// Compute contours thresh levels
	int cntMeanLVal(0);
	int nContours(0);

	for (double theta = 0.0; theta < MAX_THETA; theta += INC_THETA) {
		double dx = std::cos(theta);
		double dy = std::sin(theta);

		int rho = 0;
		int valMean = 0;

		// Init info
		int pCarth = 0;
		for (double x = center.x, y = center.y; ; x += dx, y += dy) {
			if (x < 0 || y < 0 || x >= fGray.cols || y >= fGray.rows)
				break;

			int npCarth = (int)x + ((int)y)* fGray.cols;
			if (npCarth == pCarth) continue;
			pCarth = npCarth;

			if (useMask && !mask.data[pCarth]) // outside the mask (even if not convex, osef)
				break;

			const unsigned char valGray = fGray.data[pCarth];

			// Limits
			valMean += valGray;
			rho++;
		}
		valMean /= rho;

		// Contourifie
		unsigned char lastVal = 0xFF;
		unsigned char cntLVal = 0x00;
		unsigned char cntNVal = 0x00;
		int scoreContour = 0; // worst score

		for (double x = center.x, y = center.y; ; x += dx, y += dy) {
			if (x < 0 || y < 0 || x >= fGray.cols || y >= fGray.rows)
				break;

			cv::Point pt((int)x, (int)y);
			int npCarth = pt.x + (pt.y)* fGray.cols;
			if (npCarth == pCarth) 
				continue;

			if (useMask && !mask.data[npCarth])
				break;

			unsigned char val = fGray.data[npCarth];

			bool c1 = lastVal < val;
			bool c2 = val < valMean;
			int nScore = (c1 && c2) * (val - lastVal);
			if (nScore > scoreContour) {
				scoreContour = nScore;
				cntLVal = lastVal;
			}

			// Update last pos
			pCarth    = npCarth;
			lastVal   = val;
		}

		if (scoreContour > 0) {
			cntMeanLVal += cntLVal;
			nContours++;
		}
	}
	if (nContours > 0) {
		cntMeanLVal /= nContours;
	}
	

	// Thresh contours
	cv::Mat fContours = cv::Mat::zeros(fGray.rows, fGray.cols, CV_8UC1);

	for (int y = 0; y <fGray.rows; y++) {
		for (int x = 0; x < fGray.cols; x++) {
			int p = y * fGray.cols + x;
			if (useMask && !mask.data[p])
				continue;

			if (fGray.data[p] < cntMeanLVal)
				fContours.data[p] = 255;
		}
	}

	std::vector<std::vector<cv::Point>> allContours;
	cv::findContours(fContours, allContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	// Remove too small contours
	allContours.erase (
		std::remove_if(allContours.begin(), allContours.end(), [=](const std::vector<cv::Point>& cnt) {
			return cnt.size() < 10;
		}), allContours.end()
	);

	// Select closest contour from the center;
	if (allContours.empty())
		return;

	size_t bestContourId = 0;
	int bestCost = fGray.size().area();

	for (size_t i = 0; i < allContours.size(); i++) {
		cv::Rect bdgRect = cv::boundingRect(allContours[i]);

		if (center.inside(bdgRect)) {
			bestCost = 0;
			bestContourId = i;
			break;
		}
		else {
			cv::Point vecCenterBdg(center - (bdgRect.tl() + cv::Point(bdgRect.width / 2, bdgRect.height / 2)));
			int cost = vecCenterBdg.dot(vecCenterBdg);
			if (cost < bestCost) {
				bestCost = cost;
				bestContourId = i;
			}
		}
	}

	// Finally
	contours = allContours[bestContourId];
}

void PupilleAnalysator::_extractResults(const cv::Mat& centerMap, Pupille& pupille) const {
	// --  Read algo results to approximate pupil position --
	cv::Point maxPos;
	cv::Point2f massCenter;
	int weight;

	_getResultMassCenter(centerMap, 0.8, maxPos, massCenter, weight);
	cv::Point2f centroid(massCenter * _INV_FACTOR);

	// -- Create mask
	cv::Point possibleCenterA(maxPos * _INV_FACTOR);	// cast to integer point
	cv::Point possibleCenterB(centroid);				// cast to integer point
	int maxRadiusSearched = int(1.2*std::sqrt(weight) * _INV_FACTOR);

	cv::Mat mask;
	_createMask(_eyeROI, possibleCenterA, possibleCenterB, maxRadiusSearched, mask);
	
	// .. and then find the leds
	std::vector<cv::RotatedRect> ledShapes;
	_getLeds(_eyeROI & mask, 0.8, maxRadiusSearched, ledShapes);

	// Mask led pos if near the action zone
	for (auto led : ledShapes) {
		cv::Point possibleLedCenter(led.center);
		cv::Point ledToA = possibleLedCenter - possibleCenterA;
		cv::Point ledToB = possibleLedCenter - possibleCenterB;

		int dLedToA2 = ledToA.dot(ledToA);
		int dLedToB2 = ledToB.dot(ledToB);
		int dMax = maxRadiusSearched * maxRadiusSearched; // half inside
		bool containLed = dLedToA2 < dMax || dLedToB2 < dMax;

		// Mask : circle around these points
		if (containLed) {
			cv::Point gABC = (possibleCenterA + possibleCenterB + possibleLedCenter) / 3;
			int dAG = (int)std::sqrt((gABC - possibleCenterA).dot(gABC - possibleCenterA));
			int dBG = (int)std::sqrt((gABC - possibleCenterB).dot(gABC - possibleCenterB));
			int dCG = (int)std::sqrt((gABC - possibleLedCenter).dot(gABC - possibleLedCenter));
			cv::circle(mask, gABC, std::max(std::max(dAG, dBG), dCG) + maxRadiusSearched, cv::Scalar::all(255), -1);
		}
	}

	// Contours
	_getContours(_eyeROI & mask, mask, centroid, maxRadiusSearched, pupille.contour);

	// Finally, get pupil
	cv::RotatedRect ellipsePupil;
	if(pupille.contour.size() > 6)
		ellipsePupil = cv::fitEllipse(pupille.contour);


	// Results
	bool needBlinkingCheck = false;

	if (!ellipsePupil.size.empty()) {
		pupille.shape		 = ellipsePupil;
		pupille.excentricity = std::sqrt(_computeExcentricity2(pupille.shape));
		pupille.solidity	 = _computeSolidy(pupille.contour);
		pupille.isBlinking	 = false;	// Default, assume not
		
		if (pupille.excentricity >= 0.95f) {	 // Almost sure blinking
			pupille.isBlinking = true;
		}
		else if (pupille.excentricity >= 0.6f) { // Cannot be sure using ellipse shape
			if (pupille.solidity < 0.7f) {			
				pupille.isBlinking = true;
			}
			else {							// Cannot be sure using contours shape
				needBlinkingCheck = true;
			}
		}
	}
	else {
		pupille.shape		 = cv::RotatedRect(centroid, cv::Size2f(0.9f*maxRadiusSearched, 0.9f*maxRadiusSearched), 0.0f);
		pupille.excentricity = 0;
		pupille.solidity	 = 1;
		pupille.isBlinking	 = true;			// Bad conditions anyway
	}

	// And the leds
	pupille.leds = ledShapes;


	// -- Look more precisely at blinking --
	//std::cout << "Excentricity = " << pupille.excentricity << std::endl;
	//std::cout << "solidity = " << pupille.solidity << std::endl;
	//std::cout << std::endl;

	if (needBlinkingCheck) {
		//auto beg = steady_clock::now();
		//auto end = steady_clock::now();
		//std::cout << (end - beg).count() / 1000.0 << "mus" << std::endl;
	}
}