#include "DXRSCamera.h"
#include "imgui.h"

DXRSCamera::DXRSCamera(float fieldOfView, float aspectRatio, float nearPlaneDistance, float farPlaneDistance) 
	: mFieldOfView(fieldOfView), mAspectRatio(aspectRatio), mNearPlaneDistance(nearPlaneDistance), mFarPlaneDistance(farPlaneDistance)
{

}

DXRSCamera::~DXRSCamera()
{

}

void DXRSCamera::Reset()
{
	mPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mDirection = XMFLOAT3(0.0f, 0.0f, -1.0f);
	mUp = XMFLOAT3(0.0f, 1.0f, 0.0f);
	mRight = XMFLOAT3(1.0f, 0.0f, 0.0f);

	UpdateViewMatrix();
}

void DXRSCamera::Initialize()
{
	UpdateProjectionMatrix();
	Reset();
}

void DXRSCamera::Update(DXRSTimer const& timer, U_PTR<Mouse>& aMouse, U_PTR<Keyboard>& aKeyboard)
{
	auto keyboard = aKeyboard->GetState();

	XMFLOAT3 movementAmount = XMFLOAT3(0.0f, 0.0f, 0.0f);
	{
		if (keyboard.IsKeyDown(Keyboard::W))
		{
			movementAmount.y = 1.0f;
		}

		if (keyboard.IsKeyDown(Keyboard::S))
		{
			movementAmount.y = -1.0f;
		}

		if (keyboard.IsKeyDown(Keyboard::A))
		{
			movementAmount.x = -1.0f;
		}

		if (keyboard.IsKeyDown(Keyboard::D))
		{
			movementAmount.x = 1.0f;
		}

		if (keyboard.IsKeyDown(Keyboard::E))
		{
			movementAmount.z = 1.0f;
		}

		if (keyboard.IsKeyDown(Keyboard::Q))
		{
			movementAmount.z = -1.0f;
		}
	}

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	auto mouse = aMouse->GetState();
	XMFLOAT2 rotationAmount = XMFLOAT2(0.0f, 0.0f);
	if (mouse.rightButton /*&& (!io.WantCaptureMouse)*/)
	{
		rotationAmount.x = -static_cast<float>(mouse.x - mLastMousePosition.x) * mMouseSensitivity;
		rotationAmount.y = -static_cast<float>(mouse.y - mLastMousePosition.y) * mMouseSensitivity;
	}
	mLastMousePosition.x = mouse.x;
	mLastMousePosition.y = mouse.y;
	mouse;

	float elapsedTime = timer.GetElapsedSeconds();
	XMVECTOR rotationVector = XMLoadFloat2(&rotationAmount) * mRotationRate * elapsedTime;
	XMVECTOR right = XMLoadFloat3(&mRight);


	XMMATRIX pitchMatrix = XMMatrixRotationAxis(right, XMVectorGetY(rotationVector));
	XMMATRIX yawMatrix = XMMatrixRotationY(XMVectorGetX(rotationVector));

	ApplyRotation(XMMatrixMultiply(pitchMatrix, yawMatrix));

	XMVECTOR position = XMLoadFloat3(&mPosition);
	XMVECTOR movement = XMLoadFloat3(&movementAmount) * mMovementRate * elapsedTime;

	XMVECTOR strafe = right * XMVectorGetX(movement);
	position += strafe;

	XMVECTOR forward = XMLoadFloat3(&mDirection) * XMVectorGetY(movement);
	position += forward;

	XMVECTOR downup = XMLoadFloat3(&mUp) * XMVectorGetZ(movement);
	position += downup;

	XMStoreFloat3(&mPosition, position);

	UpdateViewMatrix();
}

void DXRSCamera::SetPosition(FLOAT x, FLOAT y, FLOAT z)
{
	XMVECTOR position = XMVectorSet(x, y, z, 1.0f);
	SetPosition(position);
}

void DXRSCamera::SetPosition(FXMVECTOR position)
{
	XMStoreFloat3(&mPosition, position);
}

void DXRSCamera::SetPosition(const XMFLOAT3& position)
{
	mPosition = position;
}

void DXRSCamera::SetDirection(const XMFLOAT3& direction)
{
	mDirection = direction;
}

void DXRSCamera::SetAspectRatio(float a)
{
	mAspectRatio = a;
	UpdateProjectionMatrix();
}

void DXRSCamera::SetFOV(float fov)
{
	mFieldOfView = fov;
	UpdateProjectionMatrix();
}

void DXRSCamera::SetNearPlaneDistance(float value)
{
	mNearPlaneDistance = value;
	UpdateProjectionMatrix();
}

void DXRSCamera::SetFarPlaneDistance(float value)
{
	mFarPlaneDistance = value;
	UpdateProjectionMatrix();
}

void DXRSCamera::UpdateViewMatrix()
{
	XMVECTOR eyePosition = XMLoadFloat3(&mPosition);
	XMVECTOR direction = XMLoadFloat3(&mDirection);
	XMVECTOR upDirection = XMLoadFloat3(&mUp);

	XMMATRIX viewMatrix = XMMatrixLookToRH(eyePosition, direction, upDirection);
	XMStoreFloat4x4(&mViewMatrix, viewMatrix);
}

void DXRSCamera::UpdateProjectionMatrix()
{
	XMMATRIX projectionMatrix = XMMatrixPerspectiveFovRH(mFieldOfView, mAspectRatio, mNearPlaneDistance, mFarPlaneDistance);
	XMStoreFloat4x4(&mProjectionMatrix, projectionMatrix);
}

void DXRSCamera::ApplyRotation(CXMMATRIX transform)
{
	XMVECTOR direction = XMLoadFloat3(&mDirection);
	XMVECTOR up = XMLoadFloat3(&mUp);

	direction = XMVector3TransformNormal(direction, transform);
	direction = XMVector3Normalize(direction);

	up = XMVector3TransformNormal(up, transform);
	up = XMVector3Normalize(up);

	XMVECTOR right = XMVector3Cross(direction, up);
	up = XMVector3Cross(right, direction);

	XMStoreFloat3(&mDirection, direction);
	XMStoreFloat3(&mUp, up);
	XMStoreFloat3(&mRight, right);

	rotationMatrix = transform;
}

const XMFLOAT3& DXRSCamera::Position() const
{
	return mPosition;
}

const XMFLOAT3& DXRSCamera::Direction() const
{
	return mDirection;
}

const XMFLOAT3& DXRSCamera::Up() const
{
	return mUp;
}

const XMFLOAT3& DXRSCamera::Right() const
{
	return mRight;
}

XMVECTOR DXRSCamera::PositionVector() const
{
	return XMLoadFloat3(&mPosition);
}

XMVECTOR DXRSCamera::DirectionVector() const
{
	return XMLoadFloat3(&mDirection);
}

XMVECTOR DXRSCamera::UpVector() const
{
	return XMLoadFloat3(&mUp);
}

XMVECTOR DXRSCamera::RightVector() const
{
	return XMLoadFloat3(&mRight);
}

XMMATRIX DXRSCamera::ViewMatrix() const
{
	return XMLoadFloat4x4(&mViewMatrix);
}
XMFLOAT4X4 DXRSCamera::ViewMatrix4X4() const
{
	return mViewMatrix;
}

XMMATRIX DXRSCamera::ProjectionMatrix() const
{
	return XMLoadFloat4x4(&mProjectionMatrix);
}

XMFLOAT4X4 DXRSCamera::ProjectionMatrix4X4() const
{
	return mProjectionMatrix;
}