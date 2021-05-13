#pragma once

#include "Common.h"
#include "DXRSTimer.h"

class DXRSCamera
{
public:
	DXRSCamera(float fieldOfView, float aspectRatio, float nearPlaneDistance, float farPlaneDistance);
	~DXRSCamera();

	void Initialize();
	void Update(DXRSTimer const& timer, U_PTR<Mouse>& aMouse, U_PTR<Keyboard>& aKeyboard);

	void SetPosition(const XMFLOAT3& position);
	void SetPosition(FXMVECTOR position);
	void SetPosition(FLOAT x, FLOAT y, FLOAT z);
	void SetDirection(const XMFLOAT3& direction);
	void SetAspectRatio(float a);
	void SetFOV(float fov);
	void SetNearPlaneDistance(float value);
	void SetFarPlaneDistance(float value);
	void Reset();
	void UpdateViewMatrix();
	void UpdateProjectionMatrix();
	void ApplyRotation(CXMMATRIX transform);
	const XMFLOAT3& Position() const;
	const XMFLOAT3& Direction() const;
	const XMFLOAT3& Up() const;
	const XMFLOAT3& Right() const;
	XMVECTOR PositionVector() const;
	XMVECTOR DirectionVector() const;
	XMVECTOR UpVector() const;
	XMVECTOR RightVector() const;
	XMMATRIX ViewMatrix() const;
	XMFLOAT4X4 ViewMatrix4X4() const;
	XMMATRIX ProjectionMatrix() const;
	XMFLOAT4X4 ProjectionMatrix4X4() const;
private:
	float mMouseSensitivity = 30.0f;
	float mRotationRate = XMConvertToRadians(1.0f);
	float mMovementRate = 50.0f;
	XMFLOAT2 mLastMousePosition;

	float mFieldOfView;
	float mAspectRatio;
	float mNearPlaneDistance;
	float mFarPlaneDistance;

	XMFLOAT3 mPosition;
	XMFLOAT3 mDirection;
	XMFLOAT3 mUp;
	XMFLOAT3 mRight;

	XMMATRIX rotationMatrix;

	XMFLOAT4X4 mViewMatrix;
	XMFLOAT4X4 mProjectionMatrix;
};
