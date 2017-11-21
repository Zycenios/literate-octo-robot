#include <linux/fb.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "unixcat.h"


#define argb(alpha, red, green, blue) ((((alpha << 24) | red << 16) | green << 8) | blue)
#define afromargb(argb) ((argb & 0xff000000)>>24)
#define rfromargb(argb) ((argb & 0x00ff0000)>>16)
#define gfromargb(argb) ((argb & 0x0000ff00)>>8)
#define bfromargb(argb) (argb & 0x000000ff)

/*
#define argb(alpha, red, green, blue) ((((red << 24) | green << 16) | blue << 8) | alpha)
#define afromargb(argb) (argb & 0x000000ff)
#define rfromargb(argb) ((argb & 0xff000000)>>24)
#define gfromargb(argb) ((argb & 0x00ff0000)>>16)
#define bfromargb(argb) ((argb & 0x0000ff00)>>8)*/

//math constants
const float root3 = 1.7320508;

//Screen dimension constants (units of pixels)
int SCREEN_WIDTH = 960;
int SCREEN_HEIGHT = 540;
const int SCREEN_DEPTH = 1300;
const float maxDepth = 80;
uint32_t* gPixels;
float* zBuffer;

//should put this somewhere else
uint32_t backgroundColor = argb(0, 0xFF, 0xFF, 0xFF);

struct mat4 {
	//default constructor makes an identity matrix. yay convenience!
	mat4()
	{
		for (int i = 0; i < 4; i++)
			for (int k = 0; k < 4; k++)
			{
				stuff[i][k] = (i == k) ? 1.0f : 0.0f;
			}
	}

	mat4(float stuff2[4][4])
	{
		for (int i = 0; i < 4; i++)
			for (int k = 0; k < 4; k++)
				stuff[i][k] = stuff2[i][k];
	}

	mat4 operator*(const mat4& right) const
	{
		float temp[4][4];
		for (int x = 0; x < 4; x++)
			for (int y = 0; y < 4; y++)
			{
				temp[x][y] = 0.0f;
				for (int i = 0; i < 4; i++)
					temp[x][y] += this->stuff[i][y] * right.stuff[x][i];
			}
		return mat4(temp);
	}

	float stuff[4][4];
};

struct vector3 {
	vector3()
	{
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
	}
	vector3(float Xi, float Yi, float Zi)
	{
		x = Xi;
		y = Yi;
		z = Zi;
	}

	vector3 operator*(const mat4& right) const
	{
		vector3 temp;
		temp.x = this->x*right.stuff[0][0] + this->y*right.stuff[0][1] + this->z*right.stuff[0][2] + right.stuff[0][3];
		temp.y = this->x*right.stuff[1][0] + this->y*right.stuff[1][1] + this->z*right.stuff[1][2] + right.stuff[1][3];
		temp.z = this->x*right.stuff[2][0] + this->y*right.stuff[2][1] + this->z*right.stuff[2][2] + right.stuff[2][3];
		return temp;
	}

	bool operator==(const vector3& right) const
	{
		return(this->x == right.x && this->y == right.y && this->z == right.z);
	}

	float x;
	float y;
	float z;
};

struct vector4 {
	vector4()
	{
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
		w = 0.0f;
	}
	vector4(float Xi, float Yi, float Zi, float Wi)
	{
		x = Xi;
		y = Yi;
		z = Zi;
		w = Wi;
	}
	vector4(float Xi, float Yi, float Zi)
	{
		x = Xi;
		y = Yi;
		z = Zi;
		w = 1.0f;
	}

	vector4 operator*(const mat4& right) const
	{
		vector4 temp;
		temp.x = this->x*right.stuff[0][0] + this->y*right.stuff[0][1] + this->z*right.stuff[0][2] + this->w*right.stuff[0][3];
		temp.y = this->x*right.stuff[1][0] + this->y*right.stuff[1][1] + this->z*right.stuff[1][2] + this->w*right.stuff[1][3];
		temp.z = this->x*right.stuff[2][0] + this->y*right.stuff[2][1] + this->z*right.stuff[2][2] + this->w*right.stuff[2][3];
		temp.w = this->x*right.stuff[3][0] + this->y*right.stuff[3][1] + this->z*right.stuff[3][2] + this->w*right.stuff[3][3];
		return temp;
	}

	vector4 operator*(const float a) const
	{
		return vector4(this->x*a, this->y*a, this->z*a, this->w*a);
	}

	vector4 operator+(const float a) const
	{
		return vector4(this->x+a, this->y+a, this->z+a, this->w+a);
	}

	vector4 operator-(const float a) const
	{
		return vector4(this->x+a, this->y+a, this->z+a, this->w+a);
	}

	vector4 operator+(const vector4& right) const
	{
		return vector4(this->x + right.x, this->y + right.y, this->z + right.z, this->w + right.w);
	}

	vector4 operator-(const vector4& right) const
	{
		return vector4(this->x - right.x, this->y - right.y, this->z - right.z, this->w - right.w);
	}

	float x;
	float y;
	float z;
	float w;
};

struct Line
{
	Line()
	{
		A = vector3();
		Acolor = vector4();
		B = vector3();
		Bcolor = vector4();
	}
	Line(vector3 Ai, vector3 Bi, vector4 Acolori, vector4 Bcolori)
	{
		if (Ai.y < Bi.y)
		{
			A = Ai;
			B = Bi;
			Acolor = Acolori;
			Bcolor = Bcolori;
		}
		else
		{
			A = Bi;
			B = Ai;
			Acolor = Bcolori;
			Bcolor = Acolori;
		}
		
		
	}
	Line(vector3 Ai, vector3 Bi, uint32_t Acolori, uint32_t Bcolori)
	{
		if (Ai.y < Bi.y)
		{
			A = Ai;
			B = Bi;
			Acolor = vector4(afromargb(Acolori), rfromargb(Acolori), gfromargb(Acolori), bfromargb(Acolori));
			Bcolor = vector4(afromargb(Bcolori), rfromargb(Bcolori), gfromargb(Bcolori), bfromargb(Bcolori));
		}
		else
		{
			A = Bi;
			B = Ai;
			Bcolor = vector4(afromargb(Acolori), rfromargb(Acolori), gfromargb(Acolori), bfromargb(Acolori));
			Acolor = vector4(afromargb(Bcolori), rfromargb(Bcolori), gfromargb(Bcolori), bfromargb(Bcolori));
		}
	}
	/*
	Line(float x1, float y1, float z1, float x2, float y2, float z2)
	{
		if (y1 < y2)
		{
			A = vector3(x1, y1, z1);
			B = vector3(x2, y2, z2);
		}
		else
		{
			B = vector3(x1, y1, z1);
			A = vector3(x2, y2, z2);
		}
	}
	*/
	Line operator*(const mat4& right) const
	{
		vector3 temp1;
		temp1.x = this->A.x*right.stuff[0][0] + this->A.y*right.stuff[0][1] + this->A.z*right.stuff[0][2] + right.stuff[0][3];
		temp1.y = this->A.x*right.stuff[1][0] + this->A.y*right.stuff[1][1] + this->A.z*right.stuff[1][2] + right.stuff[1][3];
		temp1.z = this->A.x*right.stuff[2][0] + this->A.y*right.stuff[2][1] + this->A.z*right.stuff[2][2] + right.stuff[2][3];
		vector3 temp2;
		temp2.x = this->B.x*right.stuff[0][0] + this->B.y*right.stuff[0][1] + this->B.z*right.stuff[0][2] + right.stuff[0][3];
		temp2.y = this->B.x*right.stuff[1][0] + this->B.y*right.stuff[1][1] + this->B.z*right.stuff[1][2] + right.stuff[1][3];
		temp2.z = this->B.x*right.stuff[2][0] + this->B.y*right.stuff[2][1] + this->B.z*right.stuff[2][2] + right.stuff[2][3];
		return Line(temp1, temp2, this->Acolor, this->Bcolor);
	}

	bool operator==(const Line& right) const
	{
		return(this->A == right.A && this->B == right.B);
	}

	vector3 A;
	vector4 Acolor;
	vector3 B;
	vector4 Bcolor;
};

struct Triangle
{
	Triangle()
	{
		A = vector3();
		B = vector3();
		C = vector3();
		Color = vector4();
	}
	Triangle(vector3 Ai,vector3 Bi,vector3 Ci, vector4 Colori)
	{
		A = Ai;
		B = Bi;
		C = Ci;
		Color = Colori;
	}
	Triangle operator*(const mat4& right) const
	{
		//maybe speed this up?
		return(Triangle(this->A*right, this->B*right, this->C*right, this->Color));
	}

	vector3 A;
	vector3 B;
	vector3 C;
	vector4 Color;
};

//returns a 3d translation matrix
mat4 transmat(float xOff, float yOff, float zOff)
{
	mat4 temp;
	temp.stuff[0][3] = xOff;
	temp.stuff[1][3] = yOff;
	temp.stuff[2][3] = zOff;
	return temp;
}

//returns a 3d scaling matrix
mat4 scalmat(float xScale, float yScale, float zScale)
{
	mat4 temp;
	temp.stuff[0][0] = xScale;
	temp.stuff[1][1] = yScale;
	temp.stuff[2][2] = zScale;
	return temp;
}

//returns a 3d rotation matrix in x axis around 0,0,0
//angles are in radians
mat4 rotxmat(float angle)
{
	mat4 temp;
	double tempCos = cos(angle);
	double tempSin = sin(angle);
	temp.stuff[1][1] = tempCos;
	temp.stuff[1][2] = tempSin;
	temp.stuff[2][1] = -tempSin;
	temp.stuff[2][2] = tempCos;
	return temp;
}

//returns a 3d rotation matrix in y axis around 0,0,0
//angles are in radians
mat4 rotymat(float angle)
{
	mat4 temp;
	double tempCos = cos(angle);
	double tempSin = sin(angle);
	temp.stuff[0][0] = tempCos;
	temp.stuff[2][0] = tempSin;
	temp.stuff[0][2] = -tempSin;
	temp.stuff[2][2] = tempCos;
	return temp;
}

//returns a 3d rotation matrix in z axis around 0,0,0
//angles are in radians
mat4 rotzmat(float angle)
{
	mat4 temp;
	double tempCos = cos(angle);
	double tempSin = sin(angle);
	temp.stuff[0][0] = tempCos;
	temp.stuff[0][1] = tempSin;
	temp.stuff[1][0] = -tempSin;
	temp.stuff[1][1] = tempCos;
	return temp;
}

//IMPORTANT NOTE: using row vectors, so if you apply matrix
//A, then B, then C to vector x you do 
//x' = x*A
//x'' = x'*B
//x''' = x''*C
//which is the same thing as x''' = x*A*B*C

//do the sutherland-cohen clipping thing with the binary and and all that
//1001 | 1000 | 1010
//0001 | 0000 | 0010
//0101 | 0010 | 0110
Line clipLine(Line linein, float povx, float povy)
{
	bool swapped = false;
	int temp = 0;
	//stores the vectors temporarily
	vector3 tempV1 = vector3(linein.A.x*povx, linein.A.y*povy, linein.A.z);
	vector3 tempV2 = vector3(linein.B.x*povx, linein.B.y*povy, linein.B.z);
	//stores the codes temporarily
	uint8_t temp1 = 8 * (tempV1.y > tempV1.z) + 4 * (tempV1.y < -tempV1.z) + 2 * (tempV1.x > tempV1.z) + (tempV1.x < -tempV1.z);
	uint8_t temp2 = 8 * (tempV2.y > tempV2.z) + 4 * (tempV2.y < -tempV2.z) + 2 * (tempV2.x > tempV2.z) + (tempV2.x < -tempV2.z);

	//limit set to 2 to stop infinite loops from floating point rounding errors
	while (!(temp1 == 0 && temp2 == 0) && temp < 2)
	{
		if ((temp1 & temp2) != 0)
			//return an empty line and hope it knows what that means
			return Line();
		//uh-oh. now we have to actually do math
		//swap 1 and 2; always want 1 to be the 1 outside
		if (temp1 == 0)
		{
			swapped = true;
			vector3 tempV3 = tempV1;
			tempV1 = tempV2;
			tempV2 = tempV3;
			uint8_t temp3 = temp1;
			temp1 = temp2;
			temp2 = temp3;
		}

		//now let's actually do the math
		//push to x = -z
		if ((temp1 & 1) == 1)
		{
			float t = (tempV1.z + tempV1.x) / ((tempV1.x - tempV2.x) - (tempV2.z - tempV1.z));
			tempV1.z = t*(tempV2.z - tempV1.z) + tempV1.z;
			tempV1.x = -tempV1.z;
			tempV1.y = t*(tempV2.y - tempV1.y) + tempV1.y;
		}
		//push to x = z
		else if ((temp1 & 2) == 2)
		{
			float t = (tempV1.z - tempV1.x) / ((tempV2.x - tempV1.x) - (tempV2.z - tempV1.z));
			tempV1.z = t*(tempV2.z - tempV1.z) + tempV1.z;
			tempV1.x = tempV1.z;
			tempV1.y = t*(tempV2.y - tempV1.y) + tempV1.y;
		}
		//push to y = -z
		else if ((temp1 & 4) == 4)
		{
			float t = (tempV1.z + tempV1.y) / ((tempV1.y - tempV2.y) - (tempV2.z - tempV1.z));
			tempV1.z = t*(tempV2.z - tempV1.z) + tempV1.z;
			tempV1.x = t*(tempV2.x - tempV1.x) + tempV1.x;
			tempV1.y = -tempV1.z;
		}
		//push to y = z;
		else if ((temp1 & 8) == 8)
		{
			float t = (tempV1.z - tempV1.y) / ((tempV2.y - tempV1.y) - (tempV2.z - tempV1.z));
			tempV1.z = t*(tempV2.z - tempV1.z) + tempV1.z;
			tempV1.x = t*(tempV2.x - tempV1.x) + tempV1.x;
			tempV1.y = tempV1.z;
		}
		temp1 = 8 * (tempV1.y > tempV1.z) + 4 * (tempV1.y < -tempV1.z) + 2 * (tempV1.x > tempV1.z) + (tempV1.x < -tempV1.z);
	}
	if (swapped)
		return(Line(tempV1, tempV2, linein.Bcolor, linein.Acolor));
	else
		return(Line(tempV1, tempV2, linein.Acolor, linein.Bcolor));
}

Triangle trispov(Triangle intri, float povx, float povy)
{
	Triangle temp = intri;
	temp.A.x = (povx*intri.A.x/intri.A.z + 1)*SCREEN_WIDTH/2;
	temp.A.y = SCREEN_HEIGHT - (povy*intri.A.y / intri.A.z + 1)*SCREEN_HEIGHT / 2;
	temp.B.x = (povx*intri.B.x / intri.B.z + 1)*SCREEN_WIDTH / 2;
	temp.B.y = SCREEN_HEIGHT - (povy*intri.B.y / intri.B.z + 1)*SCREEN_HEIGHT / 2;
	temp.C.x = (povx*intri.C.x / intri.C.z + 1)*SCREEN_WIDTH / 2;
	temp.C.y = SCREEN_HEIGHT - (povy*intri.C.y / intri.C.z + 1)*SCREEN_HEIGHT / 2;
	return(temp);
}

Line linespov(Line linein)
{
	Line temp = linein;
	//IMPORTANT: clipping should remove any negative OR ZERO z values
	//if z=0, divide by zero error
	temp.A.x = (linein.A.x / linein.A.z + 1)*SCREEN_WIDTH / 2;
	temp.A.y = SCREEN_HEIGHT - (linein.A.y / linein.A.z + 1)*SCREEN_HEIGHT / 2;
	temp.B.x = (linein.B.x / linein.B.z + 1)*SCREEN_WIDTH / 2;
	temp.B.y = SCREEN_HEIGHT - (linein.B.y / linein.B.z + 1)*SCREEN_HEIGHT / 2;
	return(temp);
}

vector3 avgNormals(vector3 norm1, vector3 norm2, vector3 norm3)
{
	return vector3((norm1.x + norm2.x + norm3.x) * root3 / 3, (norm1.y + norm2.y + norm3.y) * root3 / 3, (norm1.z + norm2.z + norm3.z) * root3 / 3);
}

void setPixel(uint32_t* buffer, int x, int y, uint32_t pixel)
{
	if (x < SCREEN_WIDTH && y < SCREEN_HEIGHT && x >= 0 && y >= 0)
		buffer[x + y*SCREEN_WIDTH] = pixel;
}

void drawLine(uint32_t* buffer, Line line)
{
	float xDiff = line.B.x - line.A.x;
	float yDiff = line.B.y - line.A.y;
	if (xDiff == 0.0f && yDiff == 0.0f)
	{
		setPixel(buffer, line.A.x, line.A.y, argb((unsigned int)line.Acolor.x,(unsigned int)line.Acolor.y,(unsigned int)line.Acolor.z,(unsigned int)line.Acolor.w));
		return;
	}
	if (abs(xDiff) > abs(yDiff))
	{

		float xMin = (line.A.x < line.B.x ? line.A.x : line.B.x);
		float xMax = (line.A.x < line.B.x ? line.B.x : line.A.x);
		float y1 = (line.A.x < line.B.x ? line.A.y : line.B.y);
		float slope = yDiff / xDiff;
		float y;
		for (float x = xMin; x <= xMax; x++)
		{
			y = y1 + (x - xMin)*slope;
			//implicit cast to int
			setPixel(buffer, x, y, argb((unsigned int)(line.Acolor.x - (line.Acolor.x - line.Bcolor.x)*(x - line.A.x) / xDiff), (unsigned int)(line.Acolor.y - (line.Acolor.y - line.Bcolor.y)*(x - line.A.x) / xDiff), (unsigned int)(line.Acolor.z - (line.Acolor.z - line.Bcolor.z)*(x - line.A.x) / xDiff), (unsigned int)(line.Acolor.w - (line.Acolor.w - line.Bcolor.w)*(x - line.A.x) / xDiff)));
		}
	}
	else
	{
		float yMin = (line.A.y < line.B.y ? line.A.y : line.B.y);
		float yMax = (line.A.y < line.B.y ? line.B.y : line.A.y);
		float x1 = (line.A.y < line.B.y ? line.A.x : line.B.x);
		float slope = xDiff / yDiff;
		float x;
		for (float y = yMin; y <= yMax; y++)
		{
			x = x1 + (y - yMin)*slope;
			setPixel(buffer, x, y, argb((unsigned int)(line.Acolor.x - (line.Acolor.x - line.Bcolor.x)*(y - line.A.y) / yDiff), (unsigned int)(line.Acolor.y - (line.Acolor.y - line.Bcolor.y)*(y - line.A.y) / yDiff), (unsigned int)(line.Acolor.z - (line.Acolor.z - line.Bcolor.z)*(y - line.A.y) / yDiff), (unsigned int)(line.Acolor.w - (line.Acolor.w - line.Bcolor.w)*(y - line.A.y) / yDiff)));
		}
	}
}


void trigood(uint32_t* buffer, float* zBuffer, Triangle tri)
{
	if (tri.A == tri.B || tri.A == tri.C || tri.B == tri.C)
		return;
	Line edges[3] = { Line(tri.A, tri.B, tri.Color, tri.Color), Line(tri.A, tri.C, tri.Color, tri.Color), Line(tri.B, tri.C, tri.Color, tri.Color) };
	int maxLen = 0;
	int longEdge = 0;
	for (int i = 0; i < 3; i++)
	{
		if (edges[i].B.y - edges[i].A.y >= maxLen)
		{
			maxLen = edges[i].B.y - edges[i].A.y;
			longEdge = i;
		}
	}
	if (floor(maxLen) == 0)
		return;
	int short1 = (longEdge + 1) % 3;
	int short2 = (longEdge + 2) % 3;

	//now figure out which side of the triangle the long side is on (always want to draw left to right, low to high)
	float offY;
	float offX;
	if (edges[longEdge].A == edges[short1].A || edges[longEdge].B == edges[short1].A)
	{
		offY = edges[short1].B.y;
		offX = edges[short1].B.x;
	}
	else
	{
		offY = edges[short1].A.y;
		offX = edges[short1].A.x;
	}
	//no need to check for divide by zero; no triangle if longest y difference is 0
	float oppX;
	float slopeInL = (edges[longEdge].B.x - edges[longEdge].A.x) / (edges[longEdge].B.y - edges[longEdge].A.y);
	oppX = (offY - edges[longEdge].A.y) * slopeInL + edges[longEdge].A.x;
	bool longStart = false;
	if (offX >= oppX)
		longStart = true;

	//section 1
	float longDiffY = (float)maxLen;
	float shortDiffY = (float)(edges[short1].B.y - edges[short1].A.y);

	int startEdge;
	int endEdge;
	float factorS;
	float factorStepS;
	float factorE;
	float factorStepE;
	if (floor(longDiffY) > 0 && floor(shortDiffY) > 0)
	{
		if (longStart)
		{
			startEdge = longEdge;
			endEdge = short1;
			factorS = (float)(edges[short1].A.y - edges[longEdge].A.y) / longDiffY;
			factorStepS = 1.0f / longDiffY;
			factorE = 0.0f;
			factorStepE = 1.0f / shortDiffY;
		}
		else
		{
			startEdge = short1;
			endEdge = longEdge;
			factorS = 0.0f;
			factorStepS = 1.0f / shortDiffY;
			factorE = (float)(edges[short1].A.y - edges[longEdge].A.y) / longDiffY;
			factorStepE = 1.0f / longDiffY;
		}

		float zStart;
		float zEnd;
		float z;
		int xStart;
		int xEnd;
		int x;
		for (int y = edges[short1].A.y; y < edges[short1].B.y; y++)
		{
			xStart = floor(edges[startEdge].A.x + (factorS*(edges[startEdge].B.x - edges[startEdge].A.x)));
			xEnd = floor(edges[endEdge].A.x + (factorE*(edges[endEdge].B.x - edges[endEdge].A.x)));
			if (y >= 0 && y < SCREEN_HEIGHT && xStart != xEnd)
			{
				zStart = edges[startEdge].A.z + (edges[startEdge].B.z - edges[startEdge].A.z)*factorS;
				zEnd = edges[endEdge].A.z + (edges[endEdge].B.z - edges[endEdge].A.z)*factorE;
				//loop through span (ternary stuff saves time by not writing off screen
				for (x = (xStart < 0 ? 0 : (xStart >= SCREEN_WIDTH ? SCREEN_WIDTH - 1 : xStart));
					x <= (xEnd < 0 ? 0 : (xEnd >= SCREEN_WIDTH ? SCREEN_WIDTH - 1 : xEnd));
					x++)
				{
					//z buffer stuff (displays points all the way up to eye (z > 0))
					z = zStart + (zEnd - zStart)*((x - xStart) / (xEnd - xStart));
					if (z < zBuffer[x + y*SCREEN_WIDTH] && z > 0)
					{
						setPixel(buffer, x, y, argb((unsigned int)tri.Color.x, (unsigned int)tri.Color.y, (unsigned int)tri.Color.z, (unsigned int)tri.Color.w));
						zBuffer[x + y*SCREEN_WIDTH] = z;
					}
				}
			}
			factorS += factorStepS;
			factorE += factorStepE;
		}
	}

	//section 2
	shortDiffY = (float)(edges[short2].B.y - edges[short2].A.y);
	if (floor(longDiffY) > 0 && floor(shortDiffY) > 0)
	{
		if (longStart)
		{
			startEdge = longEdge;
			endEdge = short2;
			factorS = (float)(edges[short2].A.y - edges[longEdge].A.y) / longDiffY;
			factorStepS = 1.0f / longDiffY;
			factorE = 0.0f;
			factorStepE = 1.0f / shortDiffY;
		}
		else
		{
			startEdge = short2;
			endEdge = longEdge;
			factorS = 0.0f;
			factorStepS = 1.0f / shortDiffY;
			factorE = (float)(edges[short2].A.y - edges[longEdge].A.y) / longDiffY;
			factorStepE = 1.0f / longDiffY;
		}
		int xStart;
		int xEnd;
		float zStart;
		float zEnd;
		float z;
		int x;
		for (int y = edges[short2].A.y; y < edges[short2].B.y; y++)
		{
			xStart = floor(edges[startEdge].A.x + (factorS*(edges[startEdge].B.x - edges[startEdge].A.x)));
			xEnd = floor(edges[endEdge].A.x + (factorE*(edges[endEdge].B.x - edges[endEdge].A.x)));
			if (y >= 0 && y < SCREEN_HEIGHT && xStart != xEnd)
			{
				zStart = edges[startEdge].A.z + (edges[startEdge].B.z - edges[startEdge].A.z)*factorS;
				zEnd = edges[endEdge].A.z + (edges[endEdge].B.z - edges[endEdge].A.z)*factorE;
				//loop through span (ternary stuff saves time by not writing off screen
				for (x = (xStart < 0 ? 0 : (xStart >= SCREEN_WIDTH ? SCREEN_WIDTH - 1 : xStart));
					x <= (xEnd < 0 ? 0 : (xEnd >= SCREEN_WIDTH ? SCREEN_WIDTH - 1 : xEnd));
					x++)
				{
					//z buffer stuff (displays points all the way up to eye (z > 0))
					z = zStart + (zEnd - zStart)*((x - xStart) / (xEnd - xStart));
					if (z < zBuffer[x + y*SCREEN_WIDTH] && z > 0)
					{
						setPixel(buffer, x, y, argb((unsigned int)tri.Color.x, (unsigned int)tri.Color.y, (unsigned int)tri.Color.z, (unsigned int)tri.Color.w));
						zBuffer[x + y*SCREEN_WIDTH] = z;
					}
				}
			}
			factorS += factorStepS;
			factorE += factorStepE;
		}
	}
}

int main(int argc, char* args[])
{
	//figure out the screen dimensions
	struct fb_var_screeninfo tvinfo;
	int fb_fdt = open("/dev/fb0", O_RDWR);
	//Get variable screen information
	ioctl(fb_fdt, FBIOGET_VSCREENINFO, &tvinfo);
	SCREEN_WIDTH = tvinfo.xres;
	SCREEN_HEIGHT = tvinfo.yres;
	gPixels = new uint32_t[SCREEN_WIDTH*SCREEN_HEIGHT];
	zBuffer = new float[SCREEN_WIDTH*SCREEN_HEIGHT];

	//IMPORTANT NOTE: using row vectors, so if you apply matrix
	//A, then B, then C to vector x you do 
	//x' = x*A
	//x'' = x'*B
	//x''' = x''*C
	//which is the same thing as x''' = x*A*B*C
	//perspective transformation (dimensionless fractions): Xs = (a/b)*(Xe/Ze), Ys = (a/b)*(Ye/Ze)
	//convert to coordinates: Xr = (Xs + 1)*xMax/2, Yr = yMax - (Ys + 1)*yMax/2
	float povx = SCREEN_DEPTH/SCREEN_WIDTH;
	float povy = SCREEN_DEPTH/SCREEN_HEIGHT;

	mat4 modelMat = scalmat(0.04f, 0.04f, 0.04f);
	mat4 eyeMat = transmat(0, 0, 40) * rotymat(0.0f) * scalmat(1, 1, 1);
	mat4 tempMat;

	//clipping equations:
	//if -z <= pov*x <= z and -z <= pov*y <= z, the point should be displayed

	float angle = 0;
	float step = 2.0f*3.1415f/60.0f;

	vector3 tempV1;
	vector3 tempV2;
	vector3 tempV3;
	vector3 tempNorm;
	Triangle tempTris;
	//main loop
	while(true)
	{
		//clear pixel buffer
		for (int y = 0; y < SCREEN_HEIGHT; y++)
		{
			for (int x = 0; x < SCREEN_WIDTH; x++)
			{
				gPixels[x + SCREEN_WIDTH*y] = backgroundColor;
				zBuffer[x+SCREEN_WIDTH*y] = maxDepth;
			}
		}

		//render the frame
		angle += step;
		tempMat = rotymat(angle)*modelMat*eyeMat;

		for (int i = 0; i < catIndicesLen; i += 3)
		{
			tempV1 = vector3(catVerts[i][0], catVerts[i][1], catVerts[i][2]);
			tempV2 = vector3(catVerts[i+1][0], catVerts[i+1][1], catVerts[i+1][2]);
			tempV3 = vector3(catVerts[i+2][0], catVerts[i+2][1], catVerts[i+2][2]);
			tempNorm = avgNormals(vector3(catNorms[i][0], catNorms[i][1], catNorms[i][2]) * tempMat,
				vector3(catNorms[i+1][0], catNorms[i+1][1], catNorms[i+1][2]) * tempMat,
				vector3(catNorms[i+2][0], catNorms[i+2][1], catNorms[i+2][2]) * tempMat);
			if (tempNorm.z > 0)
			{
				tempTris = Triangle(tempV1, tempV2, tempV3, vector4(0, 128, 0, 128));
				trigood(gPixels, zBuffer, trispov(tempTris*tempMat, povx, povy));
			}
		}

		//MAGIC CODE! DO NOT TOUCH
		struct fb_fix_screeninfo finfo;
		struct fb_var_screeninfo vinfo;

		int fb_fd = open("/dev/fb0", O_RDWR);

		//Get variable screen information
		ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
		vinfo.grayscale = 0;
		vinfo.bits_per_pixel = 32;
		ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
		ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);

		ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);

		long screensize = vinfo.yres_virtual * finfo.line_length;

		char *fbp = (char*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, (off_t)0);

		int x, y;

		for (x = 0; x<vinfo.xres; x++)
			for (y = 0; y<vinfo.yres; y++)
			{
				long location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) + (y + vinfo.yoffset) * finfo.line_length;
				*((uint32_t*)(fbp + location)) = gPixels[x+y*SCREEN_WIDTH];
			}
		//force the framebuffer to refresh
		vinfo.activate |= FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
		if(0 > ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo))
		{
			printf("Failed to refresh\n");
			return -1;
		}
	}
	return 0;
}
