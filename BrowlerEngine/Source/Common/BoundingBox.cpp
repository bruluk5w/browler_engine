#include "BoundingBox.h"

#include "Quaternion.h"

BRWL_NS


BBox BBox::getOBB (const Mat4& orientation) const
{
	//BRWL_CHECK(std::abs(orientation.x * orientation.x + orientation.y * orientation.y + orientation.z * orientation.z + orientation.w * orientation.w - 1.f) < 0.001f, nullptr);
	//const Quaternion invOrientation = orientation.inverse();
	const Mat4 invOrientation = inverse(orientation);
	const Vec3 halfDim = dim() * 0.5f;
	const Vec3 vRight = (right * invOrientation) * halfDim.x;
	const Vec3 top = (up * invOrientation) * halfDim.y;
	const Vec3 front = (forward * invOrientation) * halfDim.z;
	const Vec3 topRight = top + vRight;
	const Vec3 topLeft = top - vRight;
	const Vec3 bottomRight = vRight - top;
	const Vec3 bottomLeft = -vRight - top;

	Vec3 min{ 0, 0, 0 };
	Vec3 max{ 0, 0, 0 };
	Vec3 corner = topRight + front;
	storeMin(min, corner); storeMax(max, corner);
	corner = topLeft + front;
	storeMin(min, corner); storeMax(max, corner);
	corner = bottomRight + front;
	storeMin(min, corner); storeMax(max, corner);
	corner = bottomLeft + front;
	storeMin(min, corner); storeMax(max, corner);
	corner = topRight - front;
	storeMin(min, corner); storeMax(max, corner);
	corner = topLeft - front;
	storeMin(min, corner); storeMax(max, corner);
	corner = bottomRight - front;
	storeMin(min, corner); storeMax(max, corner);
	corner = bottomLeft - front;
	storeMin(min, corner); storeMax(max, corner);

    return BBox(min, max);
}

float BBox::getClosestPlaneFromDirection(const Vec3 & direction) const
{
	const Vec3 dir = normalized(direction);
	float maxDist = std::numeric_limits<float>::lowest();
	{
		const Vec3 corner(min.x, min.y, min.z);
		const float dist = corner * dir;
		if (dist > maxDist) maxDist = dist;
	}
	{
		const Vec3 corner(min.x, min.y, max.z);
		const float dist = corner * dir;
		if (dist > maxDist) maxDist = dist;
	}
	{
		const Vec3 corner(min.x, max.y, min.z);
		const float dist = corner * dir;
		if (dist > maxDist) maxDist = dist;
	}
	{
		const Vec3 corner(min.x, max.y, max.z);
		const float dist = corner * dir;
		if (dist > maxDist) maxDist = dist;
	}
	{
		const Vec3 corner(max.x, min.y, min.z);
		const float dist = corner * dir;
		if (dist > maxDist) maxDist = dist;
	}
	{
		const Vec3 corner(max.x, min.y, max.z);
		float dist = corner * dir;
		if (dist > maxDist) maxDist = dist;
	}
	{
		const Vec3 corner(max.x, max.y, min.z);
		float dist = corner * dir;
		if (dist > maxDist) maxDist = dist;
	}
	{
		const Vec3 corner(max.x, max.y, max.z);
		const float dist = corner * dir;
		if (dist > maxDist) maxDist = dist;
	}

	return maxDist;
}

BRWL_NS_END