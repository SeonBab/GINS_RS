# MCP 사용 가이드

RS 프로젝트는 Unreal Engine 5.8의 `ModelContextProtocol`과 `EditorToolset` 플러그인을 사용해 MCP 클라이언트에서 Unreal Editor 도구를 호출한다. 클라이언트는 Codex와 Claude Code 둘 다 지원하며 서버 설정은 공통이다.

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

## 클라이언트 연결

서버는 하나이고 클라이언트마다 설정 파일이 다르다. 어느 쪽이든 **Unreal Editor가 실행 중이고 서버가 떠 있어야** 도구가 보인다.

| 클라이언트 | 설정 파일 | 위치 기준 |
| --- | --- | --- |
| Codex | `.codex/config.toml` | 프로젝트 루트 |
| Claude Code | `.mcp.json` | **Claude Code를 실행한 디렉터리** |

### Codex

프로젝트 루트의 `.codex/config.toml`에 다음 항목을 추가한다.

```toml
[mcp_servers.unreal-mcp]
url = "http://127.0.0.1:8000/mcp"
```

Unreal Editor 콘솔에서 설정 파일 생성을 요청할 수도 있다.

```text
ModelContextProtocol.GenerateClientConfig Codex
```

`.codex/config.toml`이 이미 존재하면 Unreal 5.8은 파일을 덮어쓰지 않으므로 서버 항목을 직접 추가한다.

### Claude Code

Claude Code는 **실행한 디렉터리의 `.mcp.json`** 을 읽는다. 그 디렉터리는 작업자마다 다르므로 이 문서는 위치를 지정하지 않는다. 실행 디렉터리와 무관하게 쓰려면 사용자 범위로 등록한다.

```json
{
	"mcpServers": {
		"unreal-mcp": {
			"type": "http",
			"url": "http://127.0.0.1:8000/mcp"
		}
	}
}
```

CLI로 등록해도 된다. 실행 디렉터리에 상관없이 쓰려면 사용자 범위를 사용한다.

```bash
claude mcp add --transport http unreal-mcp http://127.0.0.1:8000/mcp
claude mcp add --scope user --transport http unreal-mcp http://127.0.0.1:8000/mcp
```

`.mcp.json`은 Codex의 `.codex/config.toml`과 달리 저장소에 두어도 공유되지 않을 수 있다. 이 저장소의 루트는 `GINS_RS`이고 Claude Code를 그 상위에서 실행하면 설정 파일이 저장소 밖에 놓이기 때문이다. 팀에서 공유할 필요가 있으면 실행 디렉터리를 `GINS_RS`로 맞추고 그 안에 `.mcp.json`을 두거나, 각자 사용자 범위로 등록한다.

설정을 추가한 뒤에는 **Claude Code 세션을 다시 시작해야** 도구가 목록에 나타난다. 실행 중인 세션에는 반영되지 않는다.

포트나 URL 경로를 변경한 경우 Unreal Editor와 클라이언트 설정을 동일하게 맞춘다.

## 확인 및 주의사항

- Unreal Editor가 실행 중이어야 MCP 도구를 사용할 수 있다. 에디터를 닫으면 도구 호출이 실패한다.
- 연결 여부는 서버 응답으로 먼저 확인할 수 있다. 응답이 없으면 에디터가 꺼져 있거나 서버가 시작되지 않은 것이다.

```bash
curl -s -o /dev/null -w "%{http_code}" http://127.0.0.1:8000/mcp
```

- 연결되지 않으면 Output Log의 `LogModelContextProtocol` 메시지와 포트 충돌을 확인한다.
- 도구가 보이지 않으면 `ModelContextProtocol.RefreshTools`를 실행한다.
- 에셋 저장, 액터 삭제, 프로퍼티 변경 전에는 대상과 범위를 확인한다.
- MCP 서버는 개발 중에만 실행하고 외부 네트워크에 포트를 노출하지 않는다.
- 엔진 버전을 변경하면 플러그인 설정과 MCP 연결을 다시 검증한다.
