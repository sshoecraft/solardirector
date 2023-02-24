/**
* KalmanFilter
* @class
* @author Wouter Bulten
* @see {@link http://github.com/wouterbulten/kalmanjs}
* @version Version: 1.0.0-beta
* @copyright Copyright 2015-2018 Wouter Bulten
* @license MIT License
* @preserve
*/

/**
* Create 1-dimensional kalman filter
* @param  {Number} options.R Process noise
* @param  {Number} options.Q Measurement noise
* @param  {Number} options.A State vector
* @param  {Number} options.B Control vector
* @param  {Number} options.C Measurement vector
* @return {KalmanFilter}
*/
//KalmanFilter.prototype.constructor({R: 1, Q: 1, A: 1, B:  0, C: 1} = {}) {
//{ start = 5, end = 1, step = -1 } = {}
function KalmanFilter(R,Q,A,B,C) {

//	printf("R: %s, Q: %s, A: %s, B: %s, C: %s\n", R, Q, A, B, C);

	this.R = R || 1; // noise power desirable
	this.Q = Q || 1; // noise power estimated

	this.A = A || 1; // model coefficient
	this.C = C || 1;
	this.B = B || 0; // model offset

//	printf("R: %s, Q: %s, A: %s, B: %s, C: %s\n", this.R, this.Q, this.A, this.B, this.C);

	this.cov = NaN;
	this.x = NaN; // estimated signal without noise
}

KalmanFilter.prototype.reset = function() {
	this.cov = NaN;
	this.x = NaN;
}

/**
* Filter a new value
* @param  {Number} z Measurement
* @param  {Number} u Control
* @return {Number}
*/
KalmanFilter.prototype.filter = function(z, u) {

	if (typeof(u) == "undefined") u = 0;

	if (isNaN(this.x)) {
		this.x = (1 / this.C) * z;
		this.cov = (1 / this.C) * this.Q * (1 / this.C);
	} else {
		// Compute prediction
		const predX = this.predict(u);
		const predCov = this.uncertainty();

		// Kalman gain
		const K = predCov * this.C * (1 / ((this.C * predCov * this.C) + this.Q));

		// Correction
		this.x = predX + K * (z - (this.C * predX));
		this.cov = predCov - (K * this.C * predCov);
	}

	return this.x;
}

/**
* Predict next value
* @param  {Number} [u] Control
* @return {Number}
*/
KalmanFilter.prototype.predict = function(u) {
	if (typeof(u) == "undefined") u = 0;
	return (this.A * this.x) + (this.B * u);
}
  
/**
* Return uncertainty of filter
* @return {Number}
*/
KalmanFilter.prototype.uncertainty = function() {
	return ((this.A * this.cov) * this.A) + this.R;
}

/**
* Return the last filtered measurement
* @return {Number}
*/
KalmanFilter.prototype.lastMeasurement = function() {
	return this.x;
}

/**
* Set measurement noise Q
* @param {Number} noise
*/
KalmanFilter.prototype.setMeasurementNoise = function(noise) {
	this.Q = noise;
}

/**
* Set the process noise R
* @param {Number} noise
*/
KalmanFilter.prototype.setProcessNoise = function(noise) {
	this.R = noise;
}
