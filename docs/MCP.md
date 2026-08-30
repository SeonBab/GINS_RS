# MCP 사용 가이드

RS 프로젝트는 Unreal Engine 5.8의 `ModelContextProtocol`과 `EditorToolset` 플러그인을 사용해 MCP 클라이언트에서 Unreal Editor 도구를 호출한다.

두 플러그인은 실험 기능이며 `RS/RS.uproject`에서 활성화되어 있다.

## 서버 설정

Unreal Editor의 설정에서 `Model Context Protocol`을 검색해 다음 값을 확인한다.

| 설정 | 권장값 |
| --- | --- |
| Server URL Path | `/mcp` |
| Server Port Number | `8000` |
| Auto Start Server | 활성화 |
| Enable Tool Search | 활성화 |

이 설정은 `RS/Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`에 저장되므로 팀원마다 한 번씩 설정해야 한다.

자동 시작을 사용하지 않는 경우 Unreal Editor 콘솔에서 서버를 제어한다.

```text
ModelContextProtocol.StartServer
ModelContextProtocol.StopServer
ModelContextProtocol.RefreshTools
```

## Codex 연결

프로젝트 루트의 `.codex/config.toml`에 다음 항목을 추가한다.

```toml
[mcp_servers.unreal-mcp]
url = "http://127.0.0.1:8000/mcp"
```

Unreal Editor 콘솔에서 설정 파일 생성을 요청할 수도 있다.

```text
ModelContextProtocol.GenerateClientConfig Codex
```

`.codex/config.toml`이 이미 존재하면 Unreal 5.8은 파일을 덮어쓰지 않으므로 서버 항목을 직접 추가한다. 포트나 URL 경로를 변경한 경우 Unreal Editor와 Codex의 설정을 동일하게 맞춘다.

## 확인 및 주의사항

- Unreal Editor가 실행 중이어야 MCP 도구를 사용할 수 있다.
- 연결되지 않으면 Output Log의 `LogModelContextProtocol` 메시지와 포트 충돌을 확인한다.
- 도구가 보이지 않으면 `ModelContextProtocol.RefreshTools`를 실행한다.
- 에셋 저장, 액터 삭제, 프로퍼티 변경 전에는 대상과 범위를 확인한다.
- MCP 서버는 개발 중에만 실행하고 외부 네트워크에 포트를 노출하지 않는다.
- 엔진 버전을 변경하면 플러그인 설정과 MCP 연결을 다시 검증한다.
