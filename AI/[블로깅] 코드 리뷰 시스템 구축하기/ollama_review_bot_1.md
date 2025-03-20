
# 사내 코드 리뷰 자동화 시스템 구축하기 (w/ ollama) 1편 - PoC

## 1. 소개 (Introduction)

### 배경 및 필요성

 기존 수동으로 진행되고 있는 코드 리뷰 시스템에 LLM 기반 자동 코드 리뷰 시스템이 추가된다면, 리뷰어의 부담도 줄고 코드 리뷰의 퀄리티도 증가할 것이라고 판단되어 LLM 코드 리뷰 시스템 개발을 진행하게 되었습니다.

처음에는 OpenAI의 API를 활용하여 시스템 구축을 시도했으나 다음과 같은 몇 가지 주요 문제가 발생하였습니다.

- **보안 이슈** : 오픈 AI에 따르면 API 호출의 경우 별도로 데이터를 학습하지 않지만, 그럼에도 불구하고 회사 내부 코드가 외부 API로 전송되는 것에 대한 보안 부담이 존재합니다.
- **비용 문제** : API 호출이 많아질수록 운영 비용이 증가합니다. Free tier 진행 시 Rate Limit 문제가 빈번히 발생하였고 재시도 로직을 추가하였음에도 전체 MR에 대한 리뷰를 보장하기 어려웠습니다. Tier를 높이면 이런 문제가 일정 부분 해소될 수 있겠지만, 비용 부담이 발생합니다. 마일스톤 내 대규모 기능 개발이 포함된다면 이러한 부담은 점점 커집니다.

이러한 문제를 해결하기 위해, 오픈 소스 LLM 모델을 활용하여 사내 로컬 환경에서 코드 리뷰 자동화 시스템을 구축하는 방법을 선택하게 되었습니다. Ollama를 사용하면 다양한 오픈 소스 LLM을 쉽게 로드하고 내부 서버에서 실행할 수 있기 때문에 보안성과 비용 절감 측면에서 유리합니다.

### 본 글의 목적

본 글에서는 Ollama 기반의 코드 리뷰 자동화 시스템 구축 과정을 공유하고, CI/CD와의 연동 방법 및 실제 운영 시 발생한 문제와 해결 방법에 대해 설명합니다. 또한, RAG(Retrieval-Augmented Generation) 방식을 활용하여 사내 여러 가이드 문서를 검색할 수 있도록 개선한 사례를 다룹니다.

----------

## 2. Ollama와 오픈 소스 모델 살펴보기

### Ollama란?

Ollama는 다양한 오픈 소스 LLM 모델을 손쉽게 다운로드하고 실행할 수 있는 프레임워크로, 주요 기능은 다음과 같습니다.

- 다양한 LLM(llama, gemma, deepseek 등)을 지원
- 로컬 환경에서 모델 실행 가능

### 오픈 소스 모델 선택 기준

팀의 코드 리뷰 시스템에 적합한 모델을 선택하기 위해 아래 기준을 적용하였습니다. PoC 단계에서 오픈 소스 모델은 7B 파라미터의 경량 모델을 기준을 사용하였으며, 이러한 경량 모델에서의 성능을 고려했을 때 다음과 같은 기준으로 모델을 선정하였습니다.

| 모델명  | 특징  | 한국어 이해도 | dart 코드 이해도 |
| ---------- | ------------- | ------ | ----- |
| **exaone** | LG의 오픈 소스 LLM| 높음 |  준수 |
| **gemma:7b**  | 구글의 경량 오픈소스 모델 | 보통 | 낮음~보통  |
| **codellama:7b** | 메타의 코딩 특화 오픈 소스 LLM | 낮음 | 낮음  |
| **codeqwen:7b** | 알리바바에서 공개한 LLM | 보통 | 보통 |

- **선정 모델**: `exaone` (한국어 지원이 강점이며, 코드 이해도도 준수하였음.)
선정 모델은 변경 가능성이 있으며, 정성 테스트를 거쳐 최종 선정 예정입니다.

----------

## 3. Ollama 설치 및 모델 배포

### Ollama 설치 및 실행

#### 3.1. Ollama 설치

- ollama.com/download 에서 Ollama 설치

#### 3.2. 모델 실행

##### 3.2.1. Ollama cloud에서 지원하는 모델의 경우

```bash
ollama pull codellama
ollama run codellama
```

##### 3.2.2. 커스텀 된 모델을 적용하려는 경우

##### 3.2.2.1. gguf 확장자 모델 설치

```bash
# (optional) install huggingface_hub
pip install huggingface_hub

# Download the GGUF weights
huggingface-cli download LGAI-EXAONE/EXAONE-3.5-7.8B-Instruct-GGUF \
    --include "EXAONE-3.5-7.8B-Instruct-BF16*.gguf" \
    --local-dir .
```

##### 3.2.2.2. Modelfile 생성하여 gguf 파일 임포트

```bash
FROM ./EXAONE-3.5-7.8B-Instruct-BF16.gguf
```

##### 3.2.2.3. 모델 생성

```bash
ollama create model_name -f Modelfile
```

##### 3.2.2.4. 모델 실행

```bash
ollama run model_name
```

### CI/CD

```yml
ai-reviewer:
  stage: 'ai-review'
  extends: .default-python-job
  rules:
    - if: '$CI_PIPELINE_SOURCE == "merge_request_event"'
  script:
    - python3 ./script/ai-code-review/main.py
```

## 4. 코드 리뷰 자동화 시스템 개요

### 시스템 목표 및 범위

본 시스템을 구축하기에 앞서, 다음과 같은 목표를 수립하였습니다.

1. **코드 스멜 감지**: 불필요한 변수, 중복 코드 탐지하여 코드의 퀄리티 향상
2. **리팩토링 제안**: 가독성 개선, 성능 최적화 제안
3. **최적화**: CI 시스템 내 빠른 코드 통합을 위한 실행 속도 최적화

### 주요 구성 요소

- **Ollama 서버**: LLM을 로컬 빌드 머신에 호스팅
- **CI/CD 파이프라인**: MR 발생 시 Ollama에 요청
- **ChromaDB**: 사내 문서를 검색하여 가이드 제공

### 전체 다이어그램

```mermaid
flowchart LR
    D[user]
    C[CI/CD]
    R[ReviewBot]
    G[Gitlab API]
    Ch[ChromaDB]
    O[Ollama]
    
    D -- 1: merge request --> C
    C -- 2: execute --> R
    R -- 3: fetch MR (diffs) --> G
    R -- 4: search docs --> Ch
    R -- 5: request review --> O
    O -- 6: return review --> R
```

----------

## 6. 프롬프트 엔지니어링

원하는 결과를 더 정확하게 잘 이끌어내는 것을 목표로 프롬프트를 디자인하는 것을 프롬프팅이라고 합니다. 다음과 같은 여섯 가지 구성 요소로 프롬프트를 설계하였습니다.

### 1. **역할 정의(Role)**

AI의 페르소나 또는 역할을 정의합니다. 역할을 명확히 정의함으로써 어떤 태도나 전문성을 가지고 적합하게 응대해야 하는지를 설정합니다.

> 당신은 숙련된 **Flutter/Dart 소프트웨어 엔지니어**입니다.

### 2. 수행해야 할 작업 명시

수행해야 하는 특정 작업이나 목표를 설정합니다.

> 당신은 숙련된 Flutter/Dart 소프트웨어 엔지니어입니다.
> **다음 코드 변경 사항을 검토하고, 잠재적인 문제점 또는 개선 사항을 제안해주세요.**
> gitlab issue, mr description 등을 종합적으로 검토하여 의미있는 리뷰를 제안해주세요.

### 3. **지식과 정보 제공**

질문과 관련해서 참고할 만한 지식과 정보를 제공합니다. 해당 시스템은 `코드 리뷰`를 목적으로 하므로, 작성한 코드의 배경을 잘 이해할 수 있도록 Gitlab API를 통해 가져온 MR의 본문과 MR에 대응되는 issue의 본문을 컨텍스트로 제공하였습니다. 또한, RAG 기법을 활용하여 임베딩된 문서를 참고할 수 있도록 하였습니다.

> **gitlab issue, mr description 등을 종합적으로 검토하여** 리뷰를 제안해주세요.
> - 다음은 **이슈의 description**입니다:
> ...이슈 본문...
> - 다음은 **MR의 description**입니다:
> ...MR 본문...
> - 다음은 **Reference docs**입니다:
> ...Effective dart...

### 4. 정책 및 규칙, 스타일 가이드, 제약 사항 설정

응답을 만들 때 따라야하는 특정 정책이나 규칙, 스타일 가이드, 제약 사항을 설정합니다. 이는 일관된 응답을 만들어 원하는 방식으로 정보를 제공하도록 합니다.

> - comment의 경우, 참조한 docs가 존재하는 경우 출처를 밝혀주세요 (예: Effective Dart).
> - 코드 변경이 사소한 경우 (예: 변수 이름 변경, 포맷팅), 리뷰를 건너뛰어주세요.
> - 제공된 코드 변경 외의 컨텍스트가 부족하거나, 리뷰가 변경된 라인의 직접적인 내용과 무관한 경우, 리뷰를 생략하세요.
> - 제공된 코드에서 '+'로 시작하는 라인은 추가된 코드, '-'로 시작하는 라인은 삭제된 코드, 공백으로 시작하는 라인은 컨텍스트 코드입니다. 리뷰는 '+' 라인에 대해서만 작성하며, '-'와 컨텍스트 라인은 참고용으로만 사용하세요.
> - 변수가 정의되어 있는지 여부는 주어진 코드에 명시되지 않은 경우 판단하지 마세요.

### 5. 형식 및 구조 설정

응답이 따라야 하는 특정 구조를 설정합니다.

> - 응답은 마크다운을 포함하지 않는 JSON 형식으로만 생성하세요. 아래 구조를 따라주세요.
[ { "new_line": int, "comment": "review comment" }, ... ]
> - json의 key는 'new_line', 'comment' 두 가지만 포함합니다.

### 6. **출력 예시 제공**

원하는 응답 형식을 구체적으로 보여주는 예시를 제공합니다. 

```bash
[
    {
          "new_line": 87,
          "comment": "`transmission.addAll(receiver)`가 호출되는 클로저 내부의 논리를 검토합니다. 특히 여러 멤버가 관련된 경우 이 작업이 의도치 않게 다른 스트림의 데이터를 덮어쓰거나 잘못 정렬하지 않도록 주의하세요."
    },
    {
         "new_line": 91,
         "comment": "The `_intensitiesSubscription` is initialized inside `_listen` but should be scoped appropriately to ensure it is only active when needed. Consider if this subscription should be managed differently based on the context (e.g., per memberNo)."
    },
    {
          "new_line": 97,
          "comment": "Similar to the comment on line 91, `_volumeSubscription` should be managed carefully to avoid memory leaks or unintended cancellations. Ensure that subscription management aligns with the lifecycle of receiver operations."
    }
]
```

## 7. RAG(Retrieval-Augmented Generation) 적용

RAG 기술을 적용하여 모델이 사전 학습된 지식에만 의존하는 대신, 실시간 또는 특정 도메인의 데이터를 활용해 더 정확하고 맥락적인 응답을 생성할 수 있습니다. 팀의 스타일에 맞는 코딩 컨벤션 / 아키텍처 설계도 등의 문서가 존재할 경우, 이러한 문서를 참고하게끔 한다면 조금 더 맥락있는 리뷰가 가능할 것입니다.

### 7.1. 검색 단계 (Retrieval)

- **지식 기반 준비**: 외부 데이터(예: 문서, 웹 페이지, 데이터베이스)를 미리 인덱싱합니다. 이를 위해 벡터 저장소(Vector Store, 예: FAISS, Pinecone, Chorma)를 사용해 텍스트를 임베딩(Embedding)으로 변환합니다.
  - 임베딩은 사전 학습된 모델(예: BERT, Sentence-BERT)로 생성되며, 의미적 유사성을 기반으로 저장됩니다.
- **쿼리 처리**: 사용자가 질문을 입력하면, 이를 임베딩으로 변환해 지식 기반에서 관련 문서를 검색합니다.
  - 예: "Flutter에서 상태 관리 방법은?" → 관련 문서(Flutter 공식 문서, 튜토리얼)가 검색됨.
- **랭킹**: 검색된 문서 중 상위 𝑘개를 선택해 생성 단계로 전달하고, 코사인 유사도나 다른 메트릭을 사용해 랭킹.

### 7.3 생성 단계 (Generation)

- **프롬프트 증강**: 검색된 문서를 원래 질문과 결합해 프롬프트로 만듭니다.
  - 예: "다음 문서를 기반으로 답변하세요: [검색된 문서]
- **LLM 활용**: 증강된 프롬프트를 LLM에 입력해 응답을 생성합니다.

RAG 기능을 강화하기 위한 다양한 방법이 있지만, 현재 수준에서는 `검색` -> `생성 단계` 정도만을 활용한 기본적인 형태를 적용하였습니다.

----------

## 8. 마주한 문제점들

### 8.1. 정제되지 않은 무작위 응답값

생성된 응답을 적절히 활용하기 위해서는 LLM이 일관된 형태의 JSON으로 응답값을 생성해 줄 필요가 있었습니다. 다수의 조건을 명확하게 명시했음에도 불구하고 JSON 형식 이외의 응답을 뱉는 경우가 빈번했습니다.

예를 들면 다음과 같은 잘못된 응답이 빈번하게 발생하였습니다.

- **Markdown 형태의 응답값**
markdown을 포함하지 않은 json 형식의 응답값을 생성할 것을 요구했으나, 적지 않은 빈도로 다음과 같은 응답을 생성했습니다.

```markdown
#### 리뷰 내용
1. **새로운 도메인 패키지 추가 (라인 17)**
   - **라인:** 17
   - **코멘트:** `room_domain` 패키지를 추가함으로써 도메인 로직과 데이터 처리 로직을 명확히 분리하려는 의도가 보입니다. 이 변경이 데이터 소스와 관련된 로직을 효과적으로 이관하는 데 도움이 되는지 확인해야 합니다. 특히, `room_domain` 내의 클래스와 메서드들이 기존 로직과 잘 통합되어 있는지 검토해 보세요. 또한, 이 패키지의 도입이 향후 유지보수와 확장성에 어떤 영향을 미칠지 고려해 보세요.

[
    {
        "new_line": 17,
        "comment": "새로운 `room_domain` 패키지를 추가하여 도메인 로직을 분리하는 것은 좋은 접근입니다. 이 패키지 내의 구현이 기존 데이터 처리 로직과 원활하게 통합되었는지, 그리고 향후 유지보수와 확장성 측면에서 효과적인지 확인해 보세요."
    }
]

### 추가 검토 사항
1. **Rx 적용 및 Subject 사용:**
   - `XXsubject`로의 변경이 실제 데이터 흐름과 반응성에 어떤 영향을 미치는지 확인해야 합니다. 특히, 구독 관리와 스트림 처리 로직이 올바르게 구현되었는지 검토해야 합니다.
2. **데이터 소스 분리:**
   - 요청, 응답, 알림 로직이 명확하게 분리되었는지 확인합니다. 각 데이터 소스가 독립적으로 동작하면서도 필요한 경우 적절히 상호작용하는지 확인해야 합니다.
3. **테스트 커버리지:**
   - 변경된 부분에 대한 테스트 케이스가 추가되었는지 확인합니다. 특히, 새로운 데이터 소스 구조와 Rx 적용 부분에 대한 테스트가 충분히 포함되어 있는지 검토해야 합니다.

이러한 점들을 중심으로 추가적인 코드 검토와 테스트를 진행하시면 좋을 것 같습니다. 추가적인 코드 변경 사항이 있다면 그에 따른 세부적인 리뷰를 제공하겠습니다.
```

- **요구 사항 이외의 json key를 포함하는 response**
json key는 `new_line`, `comment`만을 포함하는 Array 형식의 json 형태를 요구했으나 아래와 같이 임의적으로 key를 생성하여 응답을 생성했습니다.


```json
{
  "review_points": [
    {
      "line": 32,
      "comment": "생성자에서 io 객체 초기화 시, 에러 처리를 추가하는 것이 좋습니다. 연결 실패 시 적절한 로깅이나 예외 처리가 필요합니다."
    },
    {
      "line": 59,
      "comment": "소켓 연결 후 로그인 재시도 로직이 주석 처리되어 있습니다. 필요에 따라 구현하거나 주석을 명확히 하는 것이 좋습니다."
    },
    {
      "line": 78-82,
      "comment": "이제 `_setState` 메서드가 사용되지 않으므로 삭제하는 것이 좋습니다. 상태 업데이트 로직이 `_stateSubject.add`를 통해 처리되고 있습니다."
    }
  ],
  "summary": "주요 변경 사항은 Rx 스트림을 사용한 상태 관리로 인해 기존의 `_state`와 `_stateStreamController`를 `_stateSubject`로 통합하는 방향으로 이루어졌습니다. 일관성 유지와 코드의 가독성을 위해 상태 체크와 업데이트 로직을 통일하는 것이 중요합니다. 또한, 예외 처리와 주석을 통해 코드의 명확성을 높이는 것이 좋습니다."
}
```

프롬프트에 제약 조건을 설정하고 temperature를 가장 낮은 수치를 적용했음에도 불구하고 위처럼 가공하기 어려운 형태의 응답이 생성되었습니다.

### 8.2. **의미없는 리뷰 양산**

아래 캡쳐는 실제 올라온 MR에 달린 리뷰봇의 코멘트인데, 보시다시피 실질적으로 크게 도움이 되지 않는 리뷰를 양산해내곤 했습니다.

![alt text](images/review_example.png)
![alt text](images/review_example-1.png)
![alt text](images/review_example-2.png)

리뷰봇이 생산하는 의미없는 리뷰들은 다음과 같은 문제점을 가지고 있었습니다.

- **불필요한 가정**: 프롬프트에서 "코드 변경이 사소한 경우 리뷰를 건너뛰세요"와 "제공된 코드 변경 외의 컨텍스트가 부족하거나, 리뷰가 변경된 라인과 무관한 경우 리뷰를 생략하세요"라는 조건을 명시했으나, LLM은 이를 무시하고 추가적인 가정을 언급했습니다. (ex. 변수가 정의되어있지 않다면 문제가 발생할 것입니다.)
- **구체성 부족**: 리뷰 코멘트가 다소 일반적이여서 실질적으로 도움을 주지 못하는 경우가 많았습니다.

### 8.3 응답 개선하기

#### 8.3.1 프롬프트 개선

현재 수준에서 가장 효율적이면서 간단한 방법은 단연 프롬프트를 개선하는 것입니다.

아래는 기존 프롬프트의 전문입니다.

> 당신은 숙련된 Flutter/Dart 소프트웨어 엔지니어입니다.
> **다음 코드 변경 사항을 검토하고, 잠재적인 문제점 또는 개선 사항을 제안해주세요.**
> gitlab issue, mr description 등을 종합적으로 검토하여 의미있는 리뷰를 제안해주세요.
> 
> - 다음은 **이슈의 description**입니다:
> {issue_description}
> - 다음은 **MR의 description**입니다:
> {mr_description}
> - 다음은 **Reference docs**입니다:
> {docs}
> - comment의 경우, 참조한 docs가 존재하는 경우 출처를 밝혀주세요 (예: Effective Dart).
> - 코드 변경이 사소한 경우 (예: 변수 이름 변경, 포맷팅), 리뷰를 건너뛰어주세요.
> - 제공된 코드 변경 외의 컨텍스트가 부족하거나, 리뷰가 변경된 라인의 직접적인 내용과 무관한 경우, 리뷰를 생략하세요.
> - 제공된 코드에서 '+'로 시작하는 라인은 추가된 코드, '-'로 시작하는 라인은 삭제된 코드, 공백으로 시작하는 라인은 컨텍스트 코드입니다. 리뷰는 '+' 라인에 대해서만 작성하며, '-'와 컨텍스트 라인은 참고용으로만 사용하세요.
> - 변수가 정의되어 있는지 여부는 주어진 코드에 명시되지 않은 경우 판단하지 마세요.
> - 응답은 마크다운을 포함하지 않는 JSON 형식으로만 생성하세요. 아래 구조를 따라주세요.
[ { "new_line": int, "comment": "review comment" }, ... ]
> - json의 key는 'new_line', 'comment' 두 가지만 포함합니다.
> 리뷰할 코드입니다: {patch}

기존 프롬프트에 어떤 취약점이 있는지 분석했고, 다음과 같은 문제를 도출했습니다.

- **형식 지시의 강도가 약함**
  - "응답은 마크다운을 포함하지 않는 JSON 형식으로만 생성하세요"라는 지시가 있지만, LLM이 이를 우선순위로 인식하지 못했을 수 있습니다.
- **훈련 데이터 편향**:
  - LLM이 코드 리뷰와 같은 작업에서 일반적인 패턴(예: JSON 형태가 아닌 일반적인 텍스트 리뷰 응답)으로 데이터를 학습하여, 더 광범위한 리뷰를 시도했을 가능성이 있습니다.
- **문맥 혼동**
  - 프롬프트에서 요구하는 정보가 많고 정돈되지 않아 모델이 혼란을 겪고 의도한 방식으로 답을 생성하지 못할 수 있습니다.

문제점을 토대로 먼저 프롬프트를 다음과 같이 개선하였습니다.

>  당신은 숙련된 Flutter/Dart 소프트웨어 엔지니어입니다.
다음 코드 변경 사항을 검토하고, 잠재적인 문제점 또는 개선 사항을 제안해주세요.
GitLab 이슈와 MR description을 참고하되, 코드 변경과 직접 관련된 리뷰만 제공하며, 지정된 JSON 형식으로만 응답하세요.
>
> 중요 지침:
>
> 1. 응답 형식: 반드시 아래 구조의 JSON 배열로만 응답하세요. 이외의 부가적인 말은 포함하지 마세요.
[ {{ "new_line": int, "comment": "review comment" }}, ... ]
> 2. 리뷰 범위: '+'로 시작하는 라인에 대해서만 리뷰를 작성하세요.
> 3. 변수 / 클래스 등이 실제로 정의되어 있는지 여부는 주어진 코드에 명시되지 않은 경우 판단하지 마세요.
> 4. 모든 응답은 한국어로 작성해야 합니다.
>
> 입력 데이터:
>
> - 이슈의 description: {issue_description}
> - MR의 description: 제목: {mr_title} 설명: {mr_description}
> - Reference Docs: {docs}
> - 코드 변경 사항: {patch}
>
> 예시 응답:
>
> [   {{ "new_line": 85, "comment": "`_zipIntensityStream`을 `_combinedIntensityStream`과 같은 더 구체적인 이름으로 변경하여 전송 및 수신 강도를 결합하는 목적을 명확히 나타내는 것을 고려해 보세요." }}, {{ "new_line": 87, "comment": "`transmission.addAll(receiver)`가 호출되는 클로저 내부의 논리를 검토합니다. 특히 이 작업이 의도치 않게 스트림의 다른 데이터를 덮어쓰거나 잘못 정렬하지 않도록 합니다." }}, ]
>
> 주의: 위 예시는 참고용이며, 응답은 반드시 주어진 코드와 조건에 맞게 생성해야 합니다.

프롬프트를 이해하기 쉽고 간결하게 수정하였으며, 추가로 LLM에게 구체적인 예시를 다양하게 제공하는 퓨샷 프롬프팅 기법을 적용하였습니다. 결과적으로 무작위 응답값이 기존에 비해 확연히 줄어들었습니다. 이를 통해 사람도 잘 정리된 문서를 쉽게 이해하듯, LLM 또한 잘 정리된 글에 대한 이해도가 더 높다는 것을 알 수 있었습니다. 같은 내용의 프롬프트일지라도 제약 조건을 어떤 문단에 위치시키느냐도 응답에 영향을 미치곤 했습니다.

다만 그럼에도 불구하고 `xxx 클래스가 추가되었습니다. 이러한 클래스의 추가가 전체 프로젝트에 미칠 영향을 검토해보세요.` 혹은 `해당 변수가 정의되어있는지 확인해보세요.` 와 같은 일반적이고 도움이 되지 않는 리뷰를 완전히 제거하기 어려웠습니다.

#### 8.3.2 입력 데이터 가공

MR에서 변경된 코드는 Gitlab API을 받아올 수 있는데, 이 코드 패치는 아래와 같이 [unidiff](https://github.com/google/diff-match-patch/wiki/Unidiff) 형태로 제공됩니다. 

```bash
@@ -76,6 +76,7 @@ class RoomBloc extends Bloc<RoomEvent, RoomState> {
     on<_RoomListenMessageRequested>(_onListenMessageRequested);
     on<_RoomListenThemeChanged>(_onListenThemeChanged);
     on<_RoomListenMicrophoneChanged>(_onListenMicrophoneChanged);
+    on<_RoomListenTitleChanged>(_onListenRoomTitleChanged);
 
     add(const RoomEvent.listenInitialized());
     add(const RoomEvent.listenReactionRequested());
@@ -84,6 +85,7 @@ class RoomBloc extends Bloc<RoomEvent, RoomState> {
     add(const RoomEvent.listenTouchPositionRequested());
     add(const RoomEvent.listenThemeChanged());
     add(const RoomEvent.listenMicrophoneChanged());
+    add(const RoomEvent.listenRoomTitleChanged());
   }
 
   factory RoomBloc.create({
@@ -244,6 +246,13 @@ class RoomBloc extends Bloc<RoomEvent, RoomState> {
     );
   }
```

Gitlab API를 통해 inline comment를 남기기 위해서는 LLM이 위치의 정확한 코드 라인을 반환해야하는데, 위와 같은 형태로 코드를 넘겨줄 경우 LLM이 정확한 코드 라인을 짚지 못하여 Gitlab incline comment API 호출 시 에러가 빈번하게 발생하였습니다.

이 문제를 개선하기 위하여 단순 텍스트 형태인 Undiff를 코드 상에서 클래스 형태로 쉽게 활용 할 수 있게 만드는 PatchSet 라이브러리를 활용하였고, 이를 통해 코드 패치를 좀 더 가독성 있는 형태로 가공하여 LLM에게 제공하였고 잘못된 코드라인을 반환하는 문제를 완전히 해결할 수 있었습니다.

```bash
# 가공된 형태로 제공된 코드 패치
76:      on<_RoomListenMessageRequested>(_onListenMessageRequested);
77:      on<_RoomListenThemeChanged>(_onListenThemeChanged);
78:      on<_RoomListenMicrophoneChanged>(_onListenMicrophoneChanged);
79: +     on<_RoomListenTitleChanged>(_onListenRoomTitleChanged);
80:  
81:      add(const RoomEvent.listenInitialized());
82:      add(const RoomEvent.listenReactionRequested());
85:      add(const RoomEvent.listenTouchPositionRequested());
86:      add(const RoomEvent.listenThemeChanged());
87:      add(const RoomEvent.listenMicrophoneChanged());
88: +     add(const RoomEvent.listenRoomTitleChanged());
89:    }
90:  
91:    factory RoomBloc.create({
246:      );
247:    }
248:  
249: +   FutureOr<void> _onListenRoomTitleChanged(_RoomListenTitleChanged event, Emitter<RoomState> emit) async {
250: +     return emit.onEach(
251: +       _chatRepository.chatRoomStream.where((e) => e.topicId == roomId),
252: +       onData: (chatRoom) => emit(RoomState(state.data.copyWith(title: chatRoom.title))),
253: +     );
254: +   }
255: + 
256:    FutureOr<void> _onListenMicrophoneChanged(_RoomListenMicrophoneChanged event, Emitter<RoomState> emit) async {
257:      return emit.onEach(
258:        _mediaRepository.microphone,
```

이 역시도 프롬프트 개선의 일환이라고 볼 수 있겠습니다.

#### 8.3.3 응답값 후처리

프롬프트를 지속적으로 개선하여 마크다운 형태로 응답을 반환하는 빈도는 줄었지만, 그럼에도 불구하고 이러한 응답을 100% 보장하기는 어렵습니다. 따라서 정규식을 활용하여 응답 내 json 데이터를 뽑아내는 로직을 추가하여 기대한 형태의 응답 결과만을 다루도록 하였습니다.

```python
def extract_json(self, text: str) -> str:
    """
    Extract JSON content from the response text.

    Args:
    text (str): Raw response text.

    Returns:
        str: Extracted JSON string or empty string if not found.
    """
    logger.info(f"Raw response text for JSON extraction:\n{text}")
    match = re.search(r'\[\s*{.*}\s*\]', text, re.DOTALL)
    if match:
        return match.group(0)
    logger.warning("No JSON block found in text. Returning empty string.")
    return ""
```

#### 8.3.2 요청 스타일 개선

기존의 동작 방식은 LLM에게 코드가 주어지면, 리뷰에 해당하는 라인 번호를 의미하는 `new_line`과 해당 라인에 맞는 `comment`를 json 반환하도록 요청했고, 적절한 응답값이 온다면 이를 파싱하고 comment를 달게끔 봇을 구성하였습니다. 앞에서 언급한 것처럼 다양한 예외 처리 장치들을 두었지만 이러한 형태의 응답 구성은 LLM의 출력 형식에 따른 리스크가 크다고 생각했습니다. 따라서 전체 파일을 단일로 리뷰하고, 자유롭게 응답 형태를 구성하는 **file reivew** 타입의 요청 스타일을 추가하였습니다.

`file review` 형태의 요청은 응답을 json 형태로 요구하지 않고, LLM이 자유롭게 리뷰를 하게끔 하며 해당 파일의 마지막 라인에 전반적인 리뷰를 남기는 형태로 구성됩니다.

- **`line review` 스타일의 응답**: 같은 흐름의 코드에 라인별 리뷰 및 코멘트
![alt text](images/review_example-2.png)
![alt text](images/review_example-3.png)
![alt text](images/review_example-4.png)

- **`file reivew` 스타일의 응답**: 파일 당 하나의 리뷰 코멘트
![alt text](images/review_example-5.png)

`line review`의 경우, 라인별로 리뷰를 제시하므로 좀 더 직관적이고 빠르게 코멘트를 확인할 수 있습니다. 다만 이러한 방식의 경우 너무 많은 코멘트가 달려서 피로해지는 문제가 발생할 수 있으며 LLM의 응답 형식에 크게 의존하는 단점이 있습니다.

`file review`의 경우, 파일 당 1개의 자유로운 구성의 단일 리뷰를 생성하므로 좀 더 자세한 응답 결과를 확인할 수 있습니다. 또한 LLM의 응답 형식에 의존하지 않는 장점을 가집니다. 다만 라인별 상세 리뷰가 아니기 때문에 피드백이 특정 코드 라인에 대한 구체적인 지적보다는 코드 전체의 구조나 패턴에 초점이 맞춰질 수 있습니다.

어떤 형태로 리뷰를 받는 것이 더 도움이 될지는 상황에 따라 다르지만, 개인적으로는 라인별 리뷰보다 파일별 리뷰가 더 유용하다고 생각했습니다. `line review`는 코드 내 예외 처리 과정에서 else 문 등이 누락된 점을 짚는 등의 특정 상황에서 유용했습니다. 하지만 그 외에는 다소 일반적인 리뷰 결과를 제공하는 경향이 있었습니다. 반면 파일별 주요 변경 사항을 요약한 뒤 전반적인 개선 사항을 언급하는 `file review` 형식은 랜덤 리뷰어로 지정된 동료 개발자의 코드 이해도를 높여 빠르게 리뷰를 하는 데에 도움을 줄 수 있다고 판단했습니다.

상황에 따라 적절히 스위칭해가며 쓸 수 있도록 리뷰 스타일을 gitlab 환경 변수에 지정하였습니다. 또한 `PromptTemplates` 클래스를 통해 리뷰 스타일을 매개변수로 받고, 적절한 프롬프트를 반환할 수 있도록 하였습니다.

```python
class PromptTemplates:
    def __init__(self):
        self.templates = {
            "line_review": PromptTemplate(
                input_variables=["patch", "issue_description", "mr_title", "mr_description", "docs"],
                template=prompt.LINE_REVIEW_BASE_PROMPT + prompt.LINE_REVIEW_EXAMPLE
            ),
            "file_review": PromptTemplate(
                input_variables=["patch", "issue_description", "mr_title", "mr_description", "docs"],
                template=prompt.FILE_SUMMARY_BASE_PROMPT
            ),
            "summary": PromptTemplate(
                input_variables=["reviews"],
                template=prompt.ALL_SUMMERY_BASE_PROMPT + prompt.ALL_SUMMARY_EXAMPLE
            )
        }

    def get_prompt(self, prompt_type: str, **kwargs) -> str:
        if prompt_type not in self.templates:
            raise ValueError(f"Unsupported prompt type: {prompt_type}. Supported types: {list(self.templates.keys())}")
        template = self.templates[prompt_type]
        missing_vars = [var for var in template.input_variables if var not in kwargs]
        if missing_vars:
            raise ValueError(f"Missing required variables: {missing_vars}")
        return template.format(**kwargs)
```

----------

## 9. 결론 및 향후 계획

### 9.1. 구축 결과

- **팀내 코드 리뷰 자동화 성공**
기존 수동으로 진행되었던 코드 리뷰 시스템에 자동화된 리뷰 시스템을 구축할 수 있었습니다. 리뷰 시스템은 로직의 예외 처리 누락 여부 등을 체크하는 데에 도움이 되었습니다.
- **비용 절감 및 보안 강화 효과**
완전한 무료로 모델을 사용할 수 있기 때문에 OpenAI를 사용했을 때와 비교하여 비용을 절감할 수 있었습니다. 또한 외부 클라우드를 사용하지 않으므로 보안 효과도 뛰어납니다.
- **CI/CD 및 RAG 연동으로 활용성 증대**
로컬 빌드 머신에 통합하여 CI/CD 활용에 용이하고, RAG 구축을 통해 문맥을 바탕으로 한 리뷰가 가능했습니다.

### 9.2. 아쉬운 점

- **리뷰 성능** 
경량 모델을 돌릴 수 있는 정도의 PC 스펙으로 다소 아쉬운 리뷰 성능을 보여주었습니다. 다른 언어에 비해 비교적 성숙도가 낮은 Flutter/dart였기에 이러한 아쉬움이 더 컸습니다.

### 9.3. 개선 방향

#### 9.3.1 **Fine-tuned LLM 적용 및 모델 성능 개선**

- Flutter/Dart 중심의 코드 리뷰에 최적화된 모델이 부족한 상태로, 이를 해결하기 위해서는 다음과 같은 방안을 적용해볼 수 있습니다.
  - Flutter/dart 코드 리뷰 데이터를 활용하여 Fine-tuning 기법을 적용합니다.
  - 또한 특정한 리뷰 패턴을 학습하도록 미세 조정하여 보다 일관된 리뷰 결과를 도출하도록 합니다. 예를 들면 데이터셋을 아래 예시와 같이 구성하여, 코드 리뷰의 경우 new_line, comment 형태로 답변하게끔 학습시킴으로써 응답값의 일관성을 향상시킵니다.
  ```
  "input": "당신은 숙련된 Flutter/Dart 소프트웨어 엔지니어입니다. **다음 코드 변경 사항을 검토하고, 잠재적인 문제점 또는 개선 사항을 제안해주세요.** gitlab issue, mr description 등을 종합적으로 검토하여 의미있는 리뷰를 제안해주세요. - 다음은 **이슈의 description**입니다: '앱의 스크롤 성능 개선 요청' - 다음은 **MR의 description**입니다: 'ListView를 Bloc으로 관리하도록 변경' - comment의 경우, 참조한 docs가 존재하는 경우 출처를 밝혀주세요 (예: Effective Dart). - 코드 변경이 사소한 경우 (예: 변수 이름 변경, 포맷팅), 리뷰를 건너뛰어주세요. - 제공된 코드 변경 외의 컨텍스트가 부족하거나, 리뷰가 변경된 라인의 직접적인 내용과 무관한 경우, 리뷰를 생략하세요. - 제공된 코드에서 '+'로 시작하는 라인은 추가된 코드, '-'로 시작하는 라인은 삭제된 코드, 공백으로 시작하는 라인은 컨텍스트 코드입니다. 리뷰는 '+' 라인에 대해서만 작성하며, '-'와 컨텍스트 라인은 참고용으로만 사용하세요. - 변수가 정의되어 있는지 여부는 주어진 코드에 명시되지 않은 경우 판단하지 마세요. - 응답은 마크다운을 포함하지 않는 JSON 형식으로만 생성하세요. 아래 구조를 따라주세요. [ { \"new_line\": int, \"comment\": \"review comment\" }, ... ] - json의 key는 'new_line', 'comment' 두 가지만 포함합니다. 리뷰할 코드입니다: class ScrollBloc extends Bloc<ScrollEvent, ScrollState> { ScrollBloc() : super(ScrollState.initial()); } class MyWidget extends StatelessWidget { Widget build(BuildContext context) { - return Column(children: List.generate(100, (i) => Text(\"Item $i\"))); + return BlocProvider(create: (_) => ScrollBloc(), child: BlocBuilder<ScrollBloc, ScrollState>(builder: (context, state) { return ListView(children: List.generate(100, (i) => Text(\"Item $i\"))); })); } }",
  "output": "[{\"new_line\": 5, \"comment\": \"ListView가 BlocBuilder로 래핑되었으나, 모든 항목을 메모리에 로드하므로 스크롤 성능이 저하될 수 있습니다. ListView.builder를 사용하면 지연 로딩으로 성능이 개선됩니다 (참조: Flutter 공식 문서).\"}]"
  ```

- **대형 모델 적용**: Fine-tuning을 통해 Flutter/dart에 적합한 모델로 개선하는 것도 물론 좋지만, 여건이 된다면 PC 사양을 올려 좀 더 큰 파라미터를 가진 모델을 적용합니다.

#### 9.3.2 **리뷰 결과의 평가 및 개선 피드백 루프 구축**

현재 시스템은 리뷰를 생성하지만, 이 리뷰가 실제로 도움이 되었는지에 대한 피드백을 수집하는 기능은 부족한 상태입니다. 아래와 같이 피드백 루프를 구축한다면, 지속적인 개선이 가능할 것입니다.

- **평가 수집**: Gitlab API를 활용하여 MR의 코멘트를 가져오고, 동료 개발자들에게 👍 / 👎 과 같은 이모지를 남기게끔 합니다. 왜 좋았는지 / 별로였는지를 설명하는 추가 코멘트를 남겨준다면 더 유용할 것입니다. 다만 이는 사용자 부담을 증가시키므로 선택 사항으로 둡니다.
- **피드백 집계**: MR별, 파일별, 혹은 라인별(또는 전체)로 유용성 점수(좋아요 비율 등)를 집계하여 데이터를 생성합니다.
- **개선 피드백 루프**: 사용자가 👍을 준 리뷰 코멘트와 👎을 준 리뷰 코멘트를 데이터 세트로 만들어, LLM이 “우수한 리뷰 예시” 및 "낮은 퀄리티의 리뷰 예시"를 학습하도록 합니다.

다음 스텝은 Fine-tuning을 통한 모델 개선법에 대해 학습하고 이를 적용한 과정을 다루고자 합니다.