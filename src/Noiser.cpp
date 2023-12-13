#include "Noiser.h"
#include "MathHelpers.h"


constexpr double Smoothstep(double edge1, double edge2, double x) noexcept
{
	x = ProTerGen_clamp(0.0, 1.0, (x - edge1) / (edge2 - edge1));
	return x * x * x * (x * (x * 6.0 - 15.0) + 10.0);
}

double ProTerGen::Noiser::FBM(double x, double y, size_t octaves, double frecuency, double gain) const
{
	double value = 0.0;
	double amplitude = 1.0;
	double norm = 0.0;
	for (size_t i = 0; i < octaves; ++i)
	{
		value += amplitude * Noise(frecuency * x, frecuency * y);
		frecuency *= 2.0;
		norm += amplitude;
		amplitude *= gain;
	}
	return value / norm;
}

double ProTerGen::Noiser::FBM_turbulence(double x, double y, size_t octaves, double frecuency, double gain) const
{
	double value = 0.0;
	double amplitude = 1.0;
	double norm = amplitude;
	for (size_t i = 0; i < octaves; ++i)
	{
		double noise = Noise(frecuency * x, frecuency * y);
		noise = 1 - 2 * std::abs(0.5 - noise);
		noise = noise * noise;
		value += amplitude * noise;
		frecuency *= 2;
		norm += amplitude;
		amplitude *= gain;
	}
	return value / norm;
}

void ProTerGen::PerlinNoise::Generate(uint32_t seed)
{
	const auto begin = mPermutation.begin();
	const auto end = mPermutation.begin() + static_cast<size_t>(mPermutation.size() * 0.5);

	std::iota(begin, end, 0);

	std::default_random_engine engine(seed);

	std::shuffle(begin, end, engine);
	std::copy(begin, end, end);
}

double ProTerGen::PerlinNoise::Noise(double x, double y) const
{
	const double z = 0.314159265;
	return Noise(x, y, z);
}

double ProTerGen::PerlinNoise::Noise(double x, double y, double z) const
{
	const size_t px = static_cast<size_t>(std::floor(x)) & (static_cast<size_t>(mPermutation.size() * 0.5) - 1);
	const size_t py = static_cast<size_t>(std::floor(y)) & (static_cast<size_t>(mPermutation.size() * 0.5) - 1);
	const size_t pz = static_cast<size_t>(std::floor(z)) & (static_cast<size_t>(mPermutation.size() * 0.5) - 1);

	x -= std::floor(x);
	y -= std::floor(y);
	z -= std::floor(z);

	const double u = Fade(x);
	const double v = Fade(y);
	const double w = Fade(z);

	const size_t A = mPermutation[px] + py;
	const size_t B = mPermutation[px + 1] + py;

	const size_t AA = mPermutation[A] + pz;
	const size_t AB = mPermutation[A + 1] + pz;
	const size_t BA = mPermutation[B] + pz;
	const size_t BB = mPermutation[B + 1] + pz;

	const double corner0 = Grad(mPermutation[AA], x, y, z);
	const double corner1 = Grad(mPermutation[BA], x - 1, y, z);
	const double corner2 = Grad(mPermutation[AB], x, y - 1, z);
	const double corner3 = Grad(mPermutation[BB], x - 1, y - 1, z);

	const double corner4 = Grad(mPermutation[AA + 1], x, y, z - 1);
	const double corner5 = Grad(mPermutation[BA + 1], x - 1, y, z - 1);
	const double corner6 = Grad(mPermutation[AB + 1], x, y - 1, z - 1);
	const double corner7 = Grad(mPermutation[BB + 1], x - 1, y - 1, z - 1);

	// Linear interp between x edges
	const double lerpU12 = Lerp(u, corner0, corner1);
	const double lerpU23 = Lerp(u, corner2, corner3);

	const double lerpU45 = Lerp(u, corner4, corner5);
	const double lerpU67 = Lerp(u, corner6, corner7);

	// Bilinear interp between xy faces
	const double lerpV1234 = Lerp(v, lerpU12, lerpU23);
	const double lerpV4567 = Lerp(v, lerpU45, lerpU67);

	// Trilinear interp between xyz cube
	return 0.5 + Lerp(w, lerpV1234, lerpV4567) * 0.5;
}

constexpr double ProTerGen::PerlinNoise::Fade(double t) const
{
	return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

constexpr double ProTerGen::PerlinNoise::Lerp(double t, double a, double b) const
{
	return a + t * (b - a);
}

constexpr double ProTerGen::PerlinNoise::Grad(size_t hash, double x, double y, double z) const
{
	const size_t h = hash & 15;
	const double u = h < 8 ? x : y;
	const double v = h < 4 ? y : (h == 12 || h == 14) ? x
		: z;

	return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

void ProTerGen::VoronoiNoise::Generate(uint32_t seed)
{

}

double ProTerGen::VoronoiNoise::Noise(double x, double y) const
{
	const size_t px = static_cast<size_t>(std::floor(x));
	const size_t py = static_cast<size_t>(std::floor(y));

	const double fx = x - px;
	const double fy = y - py;

	const double smoothness = 1.0 + 63.0 * pow(1.0 - mSmoothness, 4.0);

	double va = 0.0;
	double wt = 0.0;
	for (int j = -2; j <= 2; ++j)
	{
		for (int i = -2; i <= 2; ++i)
		{
			Point p = Hash(px + i, py + j);

			const double dx = (mRandomness * i) - (fx + p.X);
			const double dy = (mRandomness * j) - (fy + p.Y);

			const double d = sqrt(dx * dx + dy * dy);
			const double w = pow(1.0 - Smoothstep(0.0, 1.414, d), smoothness);

			va += w * p.Val;
			wt += w;
		}
	}

	return va / wt;
}

void ProTerGen::VoronoiNoise::SetRandomness(double randomness)
{
	mRandomness = ProTerGen_clamp(0.0, 1.0, randomness);
}

void ProTerGen::VoronoiNoise::SetSmoothness(double smoothness)
{
	mSmoothness = ProTerGen_clamp(0.0, 1.0, smoothness);
}

ProTerGen::VoronoiNoise::Point ProTerGen::VoronoiNoise::Hash(double x, double y) const
{
	double _d = 0.0;
	Point p
	{
		.X = std::abs(modf(651651.15351 + 165.2354 * sin(sqrt(((x - 123.25895) * (x - 123.25895)) + ((y - 83.1516584) * (y - 83.1516584)))), &_d)),
		.Y = std::abs(modf(12132.36 + 5168.12 * cos(sqrt(((x - 748.2156) * (x - 748.2156)) + ((y - 7.2665156) * (y - 7.2665156)))), &_d)),
		.Val = std::abs(modf(150231.2518 + 153.1568 * sin(sqrt(((x - 30.232354) * (x - 30.232354)) + ((y - 8.656984654) * (y - 8.656984654)))), &_d)),
	};

	return p;
}
