#include "Spline.h"

BRWL_NS

namespace Splines
{
	void CentripetalCatmullRom(const Vec2& first, const Vec2* const samples, const Vec2& last, size_t numSamples, Vec2* const output, size_t numOutput, int numExpectedPoints)
	{
		if (numOutput == 0) return;

		const Vec2* a = &first;
		const Vec2* b = samples;
		const Vec2* c = numSamples == 1 ? samples : samples + 1;
		const Vec2* d = numSamples > 2 ? samples + 2 : &last;
		Vec2 delta1 = *b - *a;
		Vec2 delta2 = *c - *b;
		Vec2 delta3 = *d - *c;
		double t0 = 0;
		double t1 = std::sqrt(std::sqrt(delta1.x * delta1.x + delta1.y * delta1.y));
		double preT2 = std::sqrt(std::sqrt(delta2.x * delta2.x + delta2.y * delta2.y));
		double preT3 = std::sqrt(std::sqrt(delta3.x * delta3.x + delta3.y * delta3.y));

		for (unsigned int i = 0; i < numSamples - 1; ++i)
		{
			double t2 = preT2 + t1;
			double t3 = preT3 + t2;
			double t = t1;

			double dt = preT2 / numOutput;

			for (unsigned int j = 0; j < numOutput; ++j, t += dt)
			{

				const double t1MinusT0 = t1 - t0;
				const double t2MinusT1 = t2 - t1;
				const double t3MinusT1 = t3 - t1;
				const double t2MinusT0 = t2 - t0;
				const double t3MinusT2 = t3 - t2;
				const double t2MinusT = t2 - t;
				const double t3MinusT = t3 - t;
				const double tMinusT0 = t - t0;
				const double tMinusT1 = t - t1;
				const Vec2 A1 = (float)((t1 - t) / t1MinusT0) * *a + (float)(tMinusT0 / t1MinusT0) * *b;
				const Vec2 A2 = (float)(t2MinusT / t2MinusT1) * *b + (float)(tMinusT1 / t2MinusT1) * *c;
				const Vec2 A3 = (float)(t3MinusT / t3MinusT2) * *c + (float)((t - t2) / t3MinusT2) * *d;

				Vec2 B1 = (float)(t2MinusT / t2MinusT0) * A1 + (float)(tMinusT0 / t2MinusT0) * A2;
				Vec2 B2 = (float)(t3MinusT / t3MinusT1) * A2 + (float)(tMinusT1 / t3MinusT1) * A3;
				BRWL_CHECK(i * numOutput + j < numExpectedPoints, nullptr);
				output[i * numOutput + j] = (float)(t2MinusT / t2MinusT1) * B1 + (float)(tMinusT1 / t2MinusT1) * B2;

			}

			a = b;
			b = c;
			c = d;
			if (d - samples == numSamples - 1) d = &last;
			else ++d;


			delta1 = delta2;
			delta2 = delta3;
			delta3 = *d - *c;

			t1 = preT2;
			preT2 = preT3;
			preT3 = std::sqrt(std::sqrt(delta3.x * delta3.x + delta3.y * delta3.y));
			t2 = preT2 + t1;
			t3 = preT3 + t2;
		}

		BRWL_CHECK(numOutput * (numSamples - 1) < numExpectedPoints, nullptr);
		output[numOutput * (numSamples - 1)] = last;
	}


	void CentripetalCatmullRom(const Vec2* samples, unsigned int numSamples, Vec2* const output, unsigned int numOutput, int numExpectedPoints)
	{
		if (numSamples < 2) return;
		//if (numSamples == 3) return CentripetalCatmullRom(samples[0], &samples[1], samples[2], 1, output, numOutput);
		return CentripetalCatmullRom(samples[0], &samples[1], samples[numSamples - 1], numSamples - 2, output, numOutput, numExpectedPoints);
	}


}

BRWL_NS_END
