#include "PAL/DescriptorHeap.h"

namespace
{
	const BRWL_CHAR* getErrorMessge(D3D12_DESCRIPTOR_HEAP_TYPE type)
	{
		switch (type) {
		case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
			return BRWL_CHAR_LITERAL("Failed to create shader resource view descriptor heap.");
		case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
			return BRWL_CHAR_LITERAL("Failed to create sampler descriptor heap.");
		case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
			return BRWL_CHAR_LITERAL("Failed to create render target view descriptor heap.");
		case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
			return BRWL_CHAR_LITERAL("Failed to create depth stencil view descriptor heap.");
		default:
			BRWL_UNREACHABLE();
			return BRWL_CHAR_LITERAL("Failed to create descriptor heap of unknown/invalid type.");
		}
	}

	const BRWL_CHAR* getHeapName(D3D12_DESCRIPTOR_HEAP_TYPE type, bool isGpu)
	{
		switch (type) {
		case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
			return isGpu ? BRWL_CHAR_LITERAL("SRV DESCR HEAP GPU") : BRWL_CHAR_LITERAL("SRV DESCR HEAP CPU");
		case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
			return isGpu ? BRWL_CHAR_LITERAL("SAMPLER HEAP GPU") : BRWL_CHAR_LITERAL("SAMPLER HEAP CPU");
		case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
			return isGpu ? BRWL_CHAR_LITERAL("RTV HEAP GPU"): BRWL_CHAR_LITERAL("RTV HEAP CPU");
		case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
			return isGpu ? BRWL_CHAR_LITERAL("DSV HEAP GPU") : BRWL_CHAR_LITERAL("DSV HEAP CPU");
		default:
			BRWL_UNREACHABLE();
			return BRWL_CHAR_LITERAL("Failed to create descriptor heap of unknown/invalid type.");
		}
	}
}

BRWL_RENDERER_NS

namespace PAL
{

	DescriptorHandle::DescriptorHandle() :
		nativeHandles{ {0}, {0} },
		owningHeap(nullptr),
		offset(0),
		count(0),
		name(nullptr),
		resident(false)
	{ }

	DescriptorHandle::NativeHandles DescriptorHandle::operator[](int idx)
	{
		BRWL_EXCEPTION(count > 0 && owningHeap != nullptr, BRWL_CHAR_LITERAL("Accessing invalid descriptor"));
		BRWL_EXCEPTION(idx >= 0 && idx < count, BRWL_CHAR_LITERAL("Invalid descriptor access."));
		NativeHandles handles = nativeHandles;
		handles.cpu.ptr += owningHeap->descriptorSize * idx;
		handles.gpu.ptr += owningHeap->descriptorSize * idx;
		return handles;
	}

	void DescriptorHandle::release()
	{
		if (!owningHeap) return;
		if (count == 1)
		{
			owningHeap->releaseOne(this);
		}
		else if (count > 1)
		{
			owningHeap->releaseRange(this);
		}
		else
		{
			BRWL_CHECK(false, BRWL_CHAR_LITERAL("Invalid descriptor handle release."));
		}
	}




	DescriptorHeap::DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type) :
		heapType(type),
		descriptorSize(0),
		numOccupiedDescriptors(0),
		cpuHeap(nullptr),
		gpuHeap(nullptr),
		queueCompletedIdx(0),
		queueNextIdx(0),
		dirtyQueue(),
		lastHandle(0),
		lastFree(0),
		handles(),
		cpuOccupied(),
		gpuOccupied(),
		created(false)
	{ }

	DescriptorHeap::~DescriptorHeap()
	{
		destroy();
	}

	bool DescriptorHeap::create(ID3D12Device* device, unsigned int capacity)
	{
		std::scoped_lock(mutex);
		BRWL_EXCEPTION(!created, nullptr);
		destroy();

		descriptorSize = device->GetDescriptorHandleIncrementSize(heapType);
		numOccupiedDescriptors = 0;
		{	// cpu heap
			D3D12_DESCRIPTOR_HEAP_DESC desc = { };
			desc.Type = heapType;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			desc.NumDescriptors = capacity;
			desc.NodeMask = 0;

			if (!BRWL_VERIFY(SUCCEEDED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&cpuHeap))), getErrorMessge(heapType)))
			{
				destroy();
				return false;
			}

			cpuHeap->SetName(getHeapName(heapType, false));
		}
		
		{	// gpu heap
			D3D12_DESCRIPTOR_HEAP_DESC desc = { };
			desc.Type = heapType;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			desc.NumDescriptors = capacity;
			desc.NodeMask = 0;

			if (!BRWL_VERIFY(SUCCEEDED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&gpuHeap))), getErrorMessge(heapType)))
			{
				destroy();
				return false;
			}

			gpuHeap->SetName(getHeapName(heapType, true));
		}
		
		handles.resize(capacity);
		cpuOccupied.resize(capacity);
		std::fill(cpuOccupied.begin(), cpuOccupied.end(), -1);
		gpuOccupied.resize(capacity);
		std::fill(gpuOccupied.begin(), gpuOccupied.end(), -1);
		dirtyQueue.resize(capacity);

		created = true;

        return created;
	}

	void DescriptorHeap::destroy()
	{
		std::scoped_lock(mutex);
		created = false;
		descriptorSize = 0;
		numOccupiedDescriptors = 0;
		cpuHeap = nullptr;
		gpuHeap = nullptr;
		dirtyQueue.clear();
		handles.clear();
		cpuOccupied.clear();
		gpuOccupied.clear();
		lastHandle = 0;
		lastFree = 0;
	}

	DescriptorHandle* DescriptorHeap::allocateOne(const BRWL_CHAR* name)
	{
		std::scoped_lock(mutex);

		if (!BRWL_VERIFY(numOccupiedDescriptors < handles.size(), BRWL_CHAR_LITERAL("Cannot allocate another descriptor handle. Heap full.")))
		{
			return nullptr;
		}

		for (int i = 0; i < handles.size(); ++i)
		{
			lastHandle = (lastHandle + 1) % handles.size();
			if (handles[lastHandle].owningHeap == nullptr)
				break;
		}

		for (int i = 0; i < handles.size(); ++i)
		{
			lastFree = (lastFree + 1) % cpuOccupied.size();
			if (cpuOccupied[lastFree] == -1)
				cpuOccupied[lastFree] = lastHandle;
				break;
		}

		DescriptorHandle* handle = &handles[lastHandle];

		BRWL_CHECK(handle->owningHeap == nullptr, nullptr);
		
		D3D12_CPU_DESCRIPTOR_HANDLE cpu = cpuHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE gpu = cpuHeap->GetGPUDescriptorHandleForHeapStart();

		handle->owningHeap = this;
		handle->offset = lastFree;
		handle->count = 1;
		handle->name = name;
		handle->nativeHandles.cpu.ptr = cpu.ptr + descriptorSize * handle->offset;
		handle->nativeHandles.gpu.ptr = gpu.ptr + descriptorSize * handle->offset;
		handle->resident = false;
		
		// even though we found a free slot on the up-to-date cpu tracking array, we cannot create the descriptor
		// directly in the gpu heap because we may overwrite a descriptor which is scheduled to be moved
		std::get<bool>(dirtyQueue[handle->offset][queueNextIdx % maxFramesInFlight]) = true;
		std::get<unsigned int>(dirtyQueue[handle->offset][queueNextIdx % maxFramesInFlight]) = queueNextIdx;
		
		++numOccupiedDescriptors;

		return handle;
	}

	void DescriptorHeap::releaseOne(DescriptorHandle* handle)
	{
		std::scoped_lock(mutex);

		BRWL_EXCEPTION(handle && handle->owningHeap == this, nullptr);
		size_t idx = handle - handles.data();
		BRWL_EXCEPTION(idx > 0 && idx < handles.size(), nullptr);
		BRWL_EXCEPTION(handle->count == 1, nullptr);

		// mark as not available but don't free yet completely
		// the rest is done in notifyOldFrameCompleted possibly a few frames later
		handle->nativeHandles.cpu.ptr = handle->nativeHandles.gpu.ptr = 0;
		handle->resident = false;
		std::get<bool>(dirtyQueue[idx][queueNextIdx % maxFramesInFlight]) = true;
		std::get<unsigned int>(dirtyQueue[idx][queueNextIdx % maxFramesInFlight]) = queueNextIdx;
	}

	DescriptorHandle* DescriptorHeap::allocateRange(unsigned int n, const BRWL_CHAR* name)
	{
		std::scoped_lock(mutex);

		if (!BRWL_VERIFY(numOccupiedDescriptors  + n <= handles.size(), BRWL_CHAR_LITERAL("Cannot allocate requested descriptor range. Heap too full.")))
		{
			return nullptr;
		}

		for (int i = 0; i < handles.size(); ++i)
		{
			lastHandle = (lastHandle + 1) % handles.size();
			if (handles[lastHandle].owningHeap == nullptr)
				break;
		}

		DescriptorHandle* handle = &handles[lastHandle];
		BRWL_CHECK(handle->owningHeap == nullptr, nullptr);

		// after maintain we expect lastFree to have the index of the last occupied place in the cpu tracking array
		maintain();
		lastFree = (lastFree + 1) % handles.size();
		for (unsigned int i = 0; i < n; ++i)
		{
			BRWL_EXCEPTION(cpuOccupied[lastFree + i] == -1, nullptr);
			cpuOccupied[lastFree + i] = lastHandle;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE cpu = cpuHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE gpu = cpuHeap->GetGPUDescriptorHandleForHeapStart();

		handle->owningHeap = this;
		handle->offset = lastFree;
		handle->count = n;
		handle->name = name;
		handle->nativeHandles.cpu.ptr = cpu.ptr + descriptorSize * handle->offset;
		handle->nativeHandles.gpu.ptr = gpu.ptr + descriptorSize * handle->offset;
		handle->resident = false;
		lastFree = ((lastFree + n) % cpuOccupied.size()) - 1;
		numOccupiedDescriptors += n;

		return handle;
	}

	void DescriptorHeap::releaseRange(DescriptorHandle* handle)
	{
		std::scoped_lock(mutex);

		BRWL_EXCEPTION(handle && handle->owningHeap == this, nullptr);
		size_t idx = handle - handles.data();
		BRWL_EXCEPTION(idx > 0 && idx < handles.size(), nullptr);
		BRWL_EXCEPTION(handle->count > 1, nullptr);

		// mark as not available but don't free yet completely
		// the rest is done in notifyOldFrameCompleted possibly a few frames later
		handle->nativeHandles.cpu.ptr = handle->nativeHandles.gpu.ptr = 0;
		handle->resident = false;
		std::get<bool>(dirtyQueue[idx][queueNextIdx % maxFramesInFlight]) = true;
		std::get<unsigned int>(dirtyQueue[idx][queueNextIdx % maxFramesInFlight]) = queueNextIdx;
	}

	void DescriptorHeap::maintain()
	{
		// move everything to the beginning of the heap on the cpu side
		std::scoped_lock(mutex);
		lastFree = 0;
		for (const int& handleIdx : cpuOccupied)
		{
			if (handleIdx != -1)
			{
				DescriptorHandle& handle = handles[handleIdx];
				BRWL_EXCEPTION(handle.count > 0, nullptr);
				handle.offset = lastFree;
				std::get<bool>(dirtyQueue[lastFree][queueNextIdx % maxFramesInFlight]) = true;
				std::get<unsigned int>(dirtyQueue[lastFree][queueNextIdx % maxFramesInFlight]) = queueNextIdx;
				lastFree += handle.count;
			}
		}
		if (lastFree) --lastFree;
	}

	ID3D12DescriptorHeap* const* DescriptorHeap::getPointerAddress()
	{
		BRWL_EXCEPTION(created, BRWL_CHAR_LITERAL("Invalid operation on unitialized heap."));
		return gpuHeap.GetAddressOf();
	}

	void DescriptorHeap::notifyNewFrameStarted()
	{
		++queueNextIdx;
	}

	void DescriptorHeap::notifyOldFrameCompleted()
	{
		BRWL_CHECK(handles.size() == dirtyQueue.size(), nullptr); // todo: remove
		++queueCompletedIdx;

		BRWL_EXCEPTION(queueCompletedIdx < queueNextIdx, nullptr);
		BRWL_EXCEPTION(queueNextIdx - queueCompletedIdx < maxFramesInFlight, nullptr);
		std::scoped_lock(mutex);
		SCOPED_CPU_EVENT(0, 255, 0, "Descriptor Heap Maintain: %s", getHeapName(heapType, true));

		ComPtr<ID3D12Device> device;
		gpuHeap->GetDevice(IID_PPV_ARGS(&device));

		D3D12_CPU_DESCRIPTOR_HANDLE gpuHeapCpuStart = gpuHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHeapGpuStart = gpuHeap->GetGPUDescriptorHandleForHeapStart();

		for (int i = 0; i < handles.size(); ++i)
		{
			unsigned int completed = queueCompletedIdx % maxFramesInFlight;
			std::tuple<unsigned int, bool>& frameSlot = dirtyQueue[i][completed];
			bool& isDirty = std::get<bool>(frameSlot);
			if (isDirty)
			{
				unsigned int& frameIdx = std::get<unsigned int>(frameSlot);
				BRWL_EXCEPTION(frameIdx == queueCompletedIdx, BRWL_CHAR_LITERAL("Inconsistent dirty queue state."));
				DescriptorHandle* handle = &handles[i];
				BRWL_EXCEPTION(handle->owningHeap != nullptr, BRWL_CHAR_LITERAL("Invalid descriptor state."));
				if (!handle->resident)
				{
					// mark as actually free now
					numOccupiedDescriptors -= handle->count;
					for (unsigned int j = 0; j < handle->count; ++j)
					{
						occupied[handle->offset + j] = -1;
					}
					handle->name = nullptr;
					handle->owningHeap = nullptr;
					handle->offset = 0;
					handle->count = 0;
				}
				else
				{	
					// handle is still marked resident, so we probably want to update its location on the gpu heap
					const unsigned int oldOffset = (handle->nativeHandles.cpu.ptr - gpuHeapCpuStart.ptr) / descriptorSize;
					BRWL_EXCEPTION(oldOffset != handle->offset, nullptr);
					const unsigned int newOffset = handle->offset;

					// range copy handling overlapping ranges
					if (newOffset < oldOffset)
					{	// copy first to last so that we do not copy into your own range
						unsigned int cpyStride = (oldOffset - newOffset);
						unsigned int numCopied = 0;
						D3D12_CPU_DESCRIPTOR_HANDLE from = handle->nativeHandles.cpu;
						D3D12_CPU_DESCRIPTOR_HANDLE to = gpuHeapCpuStart;
						to.ptr += descriptorSize * (newOffset + handle->count);
						while (numCopied < handle->count)
						{
							unsigned int toCopy = (unsigned int)std::min((int64_t)cpyStride, (int64_t)handle->count - (int64_t)numCopied);
							unsigned int step = toCopy * descriptorSize;
							device->CopyDescriptorsSimple(toCopy, to, from, heapType);
							from.ptr += step;
							to.ptr += step;
							numCopied += toCopy;
						}

						handle->nativeHandles.cpu = to;
						handle->nativeHandles.gpu.ptr = gpuHeapGpuStart.ptr + handle->offset * descriptorSize;
					}
					else
					{	// copy last to first
						size_t cpyStride = (newOffset - oldOffset);
						size_t numCopied = 0;
						D3D12_CPU_DESCRIPTOR_HANDLE from = handle->nativeHandles.cpu;
						from.ptr += descriptorSize * handle->count;
						D3D12_CPU_DESCRIPTOR_HANDLE to = gpuHeapCpuStart;
						to.ptr += descriptorSize * (newOffset + handle->count);
						while (numCopied < handle->count)
						{
							unsigned int toCopy = (unsigned int)std::min((int64_t)cpyStride, (int64_t)handle->count - (int64_t)numCopied);
							size_t step = toCopy * descriptorSize;
							from.ptr -= step;
							to.ptr -= step;
							device->CopyDescriptorsSimple(toCopy, to, from, heapType);
							numCopied += toCopy;
						}

						handle->nativeHandles.cpu = to;
						handle->nativeHandles.gpu.ptr = gpuHeapGpuStart.ptr + handle->offset * descriptorSize;
					}
				}

				// rest dirtyflag
				frameIdx = 0;
				isDirty = false;
			}
		}
	}

}

BRWL_RENDERER_NS_END