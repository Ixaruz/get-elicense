
#include <cstring>
#include <cstdio>

#include <helpers/debugger.hpp>

Debugger::Debugger() {
  pmdmntInitialize();
  pminfoInitialize();

  m_pid = 0;
  m_tid = 0;

  pmdmntGetApplicationProcessId(&m_pid);
  pminfoGetProgramId(&m_tid, m_pid);
}

Debugger::Debugger(u64 title_id) {
  pmdmntInitialize();
  pminfoInitialize();

  m_pid = 0;
  m_tid = title_id;

  s32 count;
  const u32 max = (2048 * 4) / sizeof(u64);
  u64 pids [max];
  memset(pids, 0, sizeof(pids));
  svcGetProcessList(&count, pids, max);

  // for (s32 i = 0; i < count; i++)
  // {
  //   printf("PID: %08lx\n", pids[i]);
  // }

  for (s32 i = 0; i < count; i++)
  {
      m_pid = pids[i];
      u64 p_title_id;
      if (R_FAILED(pminfoGetProgramId(&p_title_id, m_pid)))
      {
        continue;
      }
      if (p_title_id == m_tid)
      {
        break;
      }
  }
  if (m_pid < 80)
  {
    printf("m_pid couldn't be determined properly (%ld)", m_pid);
  }
}

Debugger::~Debugger() {
  if (!m_dmnt)
  {
    svcContinueDebugEvent(m_debugHandle, 4 | 2 | 1, 0, 0);
    svcCloseHandle(m_debugHandle);
  }
  pminfoExit();
  pmdmntExit();
}

Result Debugger::attachToProcess() {
  if (m_debugHandle == 0 && (envIsSyscallHinted(0x60) == 1))
  {
    m_rc = svcDebugActiveProcess(&m_debugHandle, m_pid);
    if (R_SUCCEEDED(m_rc))
    {
      printf("Attached to process %ld via svc.\n", m_pid);
      m_dmnt = false;
    }
    return m_rc;
  }
  return 1;
  // return dmntchtForceOpenCheatProcess();
}
void Debugger::detatch()
{
  if (!m_dmnt)
  {
    svcContinueDebugEvent(m_debugHandle, 4 | 2 | 1, 0, 0);
    svcCloseHandle(m_debugHandle);
    m_dmnt = true;
  }
}
u64 Debugger::getRunningApplicationTID() {
  return m_tid;
}

u64 Debugger::getRunningApplicationPID() {
  return m_pid;
}

u64 Debugger::peekMemory(u64 address) {
  u64 out;
  if (m_dmnt)
    dmntchtReadCheatProcessMemory(address, &out, sizeof(u64));
  else
  {
    svcReadDebugProcessMemory(&out, m_debugHandle, address, sizeof(u64));
  }


  return out;
}

Result Debugger::pokeMemory(size_t varSize, u64 address, u64 value) {


  if (m_dmnt)
  return dmntchtWriteCheatProcessMemory(address, &value, varSize);
  else
  {
    return svcWriteDebugProcessMemory(m_debugHandle, &value, address, varSize);
  }

}
Result Debugger::pause() {
  if (m_dmnt)
  return dmntchtPauseCheatProcess();
  else
  {
    return svcBreakDebugProcess(m_debugHandle);
  }
}
Result Debugger::resume() {
  if (m_dmnt)
  return dmntchtResumeCheatProcess();
  else
  {
    return svcContinueDebugEvent(m_debugHandle, 4 | 2 | 1, 0, 0);
  }
}
Result Debugger::readMemory(void *buffer, size_t bufferSize, u64 address) {
  if (m_dmnt)
    return dmntchtReadCheatProcessMemory(address, buffer, bufferSize);
  else
  {
    return svcReadDebugProcessMemory(buffer, m_debugHandle, address, bufferSize);
  }
}

Result Debugger::writeMemory(void *buffer, size_t bufferSize, u64 address) {
  if (m_dmnt)
    return dmntchtWriteCheatProcessMemory(address, buffer, bufferSize);
  else
  {
    return svcWriteDebugProcessMemory(m_debugHandle, buffer, address, bufferSize);
  }
}

MemoryInfo Debugger::queryMemory(u64 address) {
  MemoryInfo memInfo = { 0 };
  if (m_dmnt)
    dmntchtQueryCheatProcessMemory(&memInfo, address);
  else
  {
    u32 pageinfo; // ignored
    svcQueryDebugProcessMemory(&memInfo, &pageinfo, m_debugHandle, address);
  }

  return memInfo;
}
