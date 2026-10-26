#pragma once

/** RS 모듈이 시작할 때 개발용 콘솔 명령을 등록하고 종료할 때 해제합니다 */
namespace RSConsoleCommands
{
	/** 개발용 명령을 등록합니다 */
	void Register();
	/** Register에서 등록한 명령을 이름으로 찾아 해제합니다 */
	void Unregister();
}
