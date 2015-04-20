#include "d3dApp.h"
#include "d3dUtil.h"

#include <vector>

struct vertex {
	XMFLOAT3 pos;
	XMFLOAT3 normal;
	XMFLOAT2 tex;
};

class TexturedCube : public D3DApp {
public:
	TexturedCube(HINSTANCE hInstance);
	~TexturedCube();

	// Convenience overrides for handling mouse input.
	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);

	bool Init();
	void OnResize();
	void UpdateScene(float dt);
	void DrawScene();

private:
	void FillRawGeometry();
	void BuildMesh();
	void BuildEffect();
	void LoadTexture();
	void CreateInputLayout();

private:
	std::vector<vertex> mVertices;
	std::vector<UINT> mIndices;

	ID3D11Buffer *mVertexBuffer;
	ID3D11Buffer *mIndexBuffer;
	ID3DX11Effect *mFx;

	ID3DX11EffectMatrixVariable *fxWorld;
	ID3DX11EffectMatrixVariable *fxWorldViewProj;
	ID3DX11EffectVariable *fxMaterial;
	ID3DX11EffectVariable *fxDLight;
	ID3DX11EffectVectorVariable *fxEyePosW;
	ID3DX11EffectShaderResourceVariable *fxDiffuseMap;
	ID3DX11EffectMatrixVariable *fxTexTran;

	float mTexTranAngle = 0.0f;

	ID3D11ShaderResourceView **sRV;
	UINT mCurTex;
	float mTimePerFrame;
	UINT mImgCnt;
	

	ID3D11InputLayout *mLayout;

	XMFLOAT4X4 mWorld;
	XMFLOAT4X4 mView;
	XMFLOAT4X4 mProj;

	POINT mLastPoint;
	XMFLOAT4 mEyePoint;
	DirectionalLight mDLight;
	Material mMaterial;
};

TexturedCube::TexturedCube(HINSTANCE hInstance) : D3DApp(hInstance), mVertexBuffer(NULL), mIndexBuffer(NULL), mFx(NULL), fxWorld(NULL), fxWorldViewProj(NULL), fxMaterial(NULL),
fxDLight(NULL),fxEyePosW(NULL), fxDiffuseMap(NULL), fxTexTran(NULL) {
	XMMATRIX I = XMMatrixIdentity();
	XMStoreFloat4x4(&mWorld, I);
	XMStoreFloat4x4(&mView, I);
	XMStoreFloat4x4(&mProj, I);

	mLastPoint.x = 0;
	mLastPoint.y = 0;
	mEyePoint = XMFLOAT4(0, 100, -50, 1.0);

	mDLight.Ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mDLight.Diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mDLight.Specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mDLight.Direction = XMFLOAT3(0.0f, -1.0f, 0.0f);
	mDLight.Pad = 1.0f;

	mMaterial.Ambient = XMFLOAT4(0.48f, 0.77f, 0.46f, 1.0f);
	mMaterial.Diffuse = XMFLOAT4(0.48f, 0.77f, 0.46f, 1.0f);
	mMaterial.Specular = XMFLOAT4(1.0f, 1.0f, 1.0f, 18.0f);
	mMaterial.Reflect = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	XMVECTOR eyePos = XMLoadFloat4(&mEyePoint);
	XMVECTOR tgtPos = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	XMVECTOR upVec = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX view = XMMatrixLookAtLH(eyePos, tgtPos, upVec);
	XMStoreFloat4x4(&mView, view);

	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.2f*XM_PI, AspectRatio(), 1.0f, 2000.0f);
	XMStoreFloat4x4(&mProj, proj);

	this->mTexTranAngle = 0.0f;
}

TexturedCube::~TexturedCube() {
	ReleaseCOM(mVertexBuffer);
	ReleaseCOM(mIndexBuffer);
	ReleaseCOM(mFx);
	for (int i = 0; i < mImgCnt; ++i){
		ReleaseCOM(sRV[i]);
	}
	delete[]sRV;
	ReleaseCOM(mLayout);
}

void TexturedCube::OnMouseDown(WPARAM btnState, int x, int y) {
	mLastPoint.x = x;
	mLastPoint.y = y;
	SetCapture(mhMainWnd);
}

void TexturedCube::OnMouseMove(WPARAM btnState, int x, int y) {
	if (mLastPoint.x == 0 && mLastPoint.y == 0){
		return;
	}
	if (btnState & MK_LBUTTON) {
		float scale = 150;
		float dx = (x - mLastPoint.x)/scale;
		float dy = (y - mLastPoint.y)/scale;

		XMVECTOR eyePoint = XMVectorSet(mEyePoint.x, mEyePoint.y, mEyePoint.z, 1.0f);
		XMVECTOR tgtPoint = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMVECTOR upVec = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		
		XMVECTOR det;
		XMMATRIX viewMatrix = XMMatrixLookAtLH(eyePoint, tgtPoint, upVec);
		XMMATRIX viewToWorld = XMMatrixInverse(&det, viewMatrix);

		XMVECTOR O = XMVectorSet(0.0f, 0.0f, 111.0f, 1.0f);
		XMVECTOR OW = XMVector4Transform(O, viewToWorld);
		XMFLOAT4 OW4;
		XMStoreFloat4(&OW4, OW);

		XMVECTOR axisInViewSpace = XMVectorSet(dy, dx, 0.0f, 0.0f);
		XMVECTOR axisInWorld = XMVector4Transform(axisInViewSpace, viewToWorld);

		axisInWorld = XMVector3Normalize(axisInWorld);
		float angle = -sqrtf(dx *dx + dy *dy);

		XMMATRIX rotM = XMMatrixRotationAxis(axisInWorld, angle);

		XMMATRIX world = XMLoadFloat4x4(&mWorld);
		world = XMMatrixMultiply(world, rotM);
		XMStoreFloat4x4(&mWorld, world);
	}

	mLastPoint.x = x;
	mLastPoint.y = y;
}

void TexturedCube::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

bool TexturedCube::Init() {
	if (!D3DApp::Init())
		return false;
	BuildMesh();
	BuildEffect();
	LoadTexture();
	CreateInputLayout();
	return true;
}

void TexturedCube::FillRawGeometry() {
	float cubeHalfSize = 25.0f;
	float to[4][2] = { { -1.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, -1.0f }, { -1.0f, -1.0f } };
	float texcor[4][2];
	for (int t = 0; t < 4; ++t){
		for (int u = 0; u < 2; ++u){
			texcor[t][u] = (to[t][u] + 1.0f) / 2.0f;
		}
	}

	int start, end, step;
	for (int xm = -1; xm <= 1; xm += 2) {
		if (xm == -1){
			start = 0;
			end = 4;
			step = 1;
		}
		else {
			start = 3;
			end = -1;
			step = -1;
		}


		for (int t = start; t != end; t += step) {
			XMFLOAT3 pos(xm*cubeHalfSize, to[t][0] * cubeHalfSize, to[t][1] * cubeHalfSize);
			XMFLOAT3 normal((float)xm, 0.0f, 0.0f);
			XMFLOAT2 tex(texcor[t]);

			vertex v;
			v.pos = pos;
			v.normal = normal;
			v.tex = tex;

			mVertices.push_back(v);
		}
	}

	for (int ym = -1; ym <= 1; ym += 2){
		if (ym == 1){
			start = 0;
			end = 4;
			step = 1;
		}
		else {
			start = 3;
			end = -1;
			step = -1;
		}


		for (int t = start; t != end; t += step) {
			XMFLOAT3 pos(to[t][0] * cubeHalfSize, ym*cubeHalfSize, to[t][1] * cubeHalfSize);
			XMFLOAT3 normal(0.0f, (float)ym, 0.0f);
			XMFLOAT2 tex(texcor[t]);

			vertex v;
			v.pos = pos;
			v.normal = normal;
			v.tex = tex;

			mVertices.push_back(v);
		}
	}

	for (int zm = -1; zm <= 1; zm += 2){
		if (zm == -1){
			start = 0;
			end = 4;
			step = 1;
		}
		else {
			start = 3;
			end = -1;
			step = -1;
		}

		for (int t = start; t != end; t += step) {
			XMFLOAT3 pos(to[t][0] * cubeHalfSize, to[t][1] * cubeHalfSize, zm*cubeHalfSize);
			XMFLOAT3 normal(0.0f, 0.0f, zm);
			XMFLOAT2 tex(texcor[t]);

			vertex v;
			v.pos = pos;
			v.normal = normal;
			v.tex = tex;

			mVertices.push_back(v);
		}
	}

	for (size_t i = 0; i + 4 <= mVertices.size(); i+=4){
		for (UINT t = 0; t < 3; ++t){
			mIndices.push_back(i + t);
		}
		for (UINT t = 2; t < 5; ++t) {
			int z = t % 4;
			mIndices.push_back(i + z);
		}
	}
}

void TexturedCube::BuildMesh() {
	FillRawGeometry();

	D3D11_BUFFER_DESC vbd;
	ZeroMemory(&vbd, sizeof(vbd));
	vbd.ByteWidth = sizeof(vertex)* mVertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	D3D11_SUBRESOURCE_DATA vData;
	ZeroMemory(&vData, sizeof(vData));
	vData.pSysMem = (void*)(&mVertices[0]);
	HR(md3dDevice->CreateBuffer(&vbd, &vData, &mVertexBuffer));

	D3D11_BUFFER_DESC ibd;
	ZeroMemory(&ibd, sizeof(ibd));
	ibd.ByteWidth = sizeof(UINT)* mIndices.size();
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	D3D11_SUBRESOURCE_DATA iData;
	ZeroMemory(&iData, sizeof(iData));
	iData.pSysMem = (void*)(&mIndices[0]);
	HR(md3dDevice->CreateBuffer(&ibd, &iData, &mIndexBuffer));
}

void TexturedCube::BuildEffect() {
	DWORD shaderFlag = 0;
#if defined (DEBUG) || defined(_DEBUG)
	shaderFlag |= D3D10_SHADER_DEBUG;
	shaderFlag |= D3D10_SHADER_SKIP_OPTIMIZATION;
#endif

	ID3D10Blob *compiledShader = NULL;
	ID3D10Blob *errMsg = NULL;

	HRESULT hr = D3DX11CompileFromFile(L"Color.fx", NULL, NULL, NULL, "fx_5_0", shaderFlag, NULL, NULL, &compiledShader, &errMsg, NULL);

	if (errMsg != NULL){
		MessageBoxA(NULL, (char*)errMsg->GetBufferPointer(), NULL, NULL);
		ReleaseCOM(errMsg);
	}

	if (FAILED(hr)) {
		DXTrace(__FILE__, (DWORD)__LINE__, hr, L"D3DX11CompileFromFile", true);
	}

	HR(D3DX11CreateEffectFromMemory(compiledShader->GetBufferPointer(), compiledShader->GetBufferSize(), shaderFlag, md3dDevice, &mFx));
	ReleaseCOM(compiledShader);

	fxWorld = mFx->GetVariableByName("gWorld")->AsMatrix();
	fxWorldViewProj = mFx->GetVariableByName("gWorldViewProj")->AsMatrix();
	fxMaterial = mFx->GetVariableByName("gMaterial");
	fxDLight = mFx->GetVariableByName("dLight");
	fxEyePosW = mFx->GetVariableByName("gEyePosW")->AsVector();
	fxDiffuseMap = mFx->GetVariableByName("gDiffuseMap")->AsShaderResource();
	fxTexTran = mFx->GetVariableByName("gTexTran")->AsMatrix();
}

void TexturedCube::LoadTexture() {
	mImgCnt = 120;
	sRV = new ID3D11ShaderResourceView*[mImgCnt];
	for (UINT i = 0; i < mImgCnt; ++i){
		std::wstring numStr = std::to_wstring(i+1);
		if (i +1< 10){
			numStr = std::wstring(L"0") + numStr;
		}

		if (i +1< 100) {
			numStr = std::wstring(L"0") + numStr;
		}
		std::wstring fileName = L"FireAnim/Fire" + numStr + L".bmp";

		HR(D3DX11CreateShaderResourceViewFromFile(md3dDevice, fileName.c_str(), NULL, NULL, sRV + i, NULL));
	}
	mCurTex = 0;
	mTimePerFrame = 0.0f;
}

void TexturedCube::CreateInputLayout() {
	D3D11_INPUT_ELEMENT_DESC inputDecs[3] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	D3DX11_PASS_DESC passDecs;
	mFx->GetTechniqueByName("T0")->GetPassByName("P0")->GetDesc(&passDecs);
	HR(md3dDevice->CreateInputLayout(inputDecs, 3, passDecs.pIAInputSignature, passDecs.IAInputSignatureSize, &mLayout));
}

void TexturedCube::OnResize() {
	D3DApp::OnResize();

	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25 * XM_PI, AspectRatio(), 1.0f, 2000.0f);
	XMStoreFloat4x4(&mProj, proj);
}

void TexturedCube::UpdateScene(float dt) {
	if (mTimePerFrame < 0.0333) {
		mTimePerFrame += dt;
	}
	else {
		++mCurTex;
		mCurTex %= mImgCnt;
		mTimePerFrame = 0.0f;
		mTexTranAngle += 0.02 * XM_PI;
	}
}

void TexturedCube::DrawScene(){
	md3dImmediateContext->ClearRenderTargetView(mRenderTargetView, (float*)(&Colors::Blue));
	md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	
	md3dImmediateContext->IASetInputLayout(mLayout);
	md3dImmediateContext->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	UINT vStride = sizeof(vertex);
	UINT vOffset = 0;
	md3dImmediateContext->IASetVertexBuffers(0, 1, &mVertexBuffer, &vStride, &vOffset); //param
	md3dImmediateContext->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R32_UINT, 0); //param

	XMMATRIX world = XMLoadFloat4x4(&mWorld);
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX worldViewProj = world * view * proj;

	fxWorld->SetMatrix((float*)&mWorld);
	fxWorldViewProj->SetMatrix((float*)&worldViewProj);
	fxEyePosW->SetFloatVector((float*)&mEyePoint);
	fxDLight->SetRawValue(&mDLight, 0, sizeof(mDLight));
	fxMaterial->SetRawValue(&mMaterial, 0, sizeof(mMaterial));
	fxDiffuseMap->SetResource(sRV[mCurTex]);

	XMMATRIX A = XMMatrixTranslation(-0.5f, -0.5f, 0);
	XMVECTOR axis = XMVectorSet(0, 0, -1.0f, 0);
	XMMATRIX B = XMMatrixRotationAxis(axis, mTexTranAngle);
	XMMATRIX C = XMMatrixTranslation(0.5f, 0.5f, 0);
	XMMATRIX texTran = A*B*C;
	fxTexTran->SetMatrix((float*)&texTran);

	ID3DX11EffectTechnique *tech = mFx->GetTechniqueByName("T0");
	ID3DX11EffectPass *pass = tech->GetPassByName("P0");
	pass->Apply(0, md3dImmediateContext);
	md3dImmediateContext->DrawIndexed(mIndices.size(), 0, 0);
	HR(mSwapChain->Present(0, 0));
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
	TexturedCube app(hInstance);
	if (!app.Init())
		return 0;
	return app.Run();
}