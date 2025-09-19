type Term = KamValue
type Code = List[KamInstruction]
type Stack = List[KamValue]

sealed trait KamValue
case class KamPair(fst: KamValue, snd: KamValue) extends KamValue
case class KamClosure(code: Code, env: KamValue) extends KamValue
case class KamConstant(value: Any) extends KamValue
case object KamEmpty extends KamValue

sealed trait KamInstruction
case object Push extends KamInstruction  // <
case object Swap extends KamInstruction // ,
case object Cons extends KamInstruction // >
case object Car extends KamInstruction  // Fst
case object Cdr extends KamInstruction  // Snd
case class Cur(code: Code) extends KamInstruction // Λ
case class Quote(value: Any) extends KamInstruction // '
case object App extends KamInstruction // ε

case class KamState(term: Term, code: Code, stack: Stack)

object KamMachine {
  
  def execute(initialState: KamState): Term = {
    var state = initialState
    var stepCount = 0
    
    while (state.code.nonEmpty && stepCount < 100) { 
      println(s"Шаг ${stepCount}: term=${state.term}, code=${state.code.mkString(",")}, stack=${state.stack}")
      state = step(state)
      stepCount += 1
    }
    
    if (stepCount >= 100) println("Превышено количество шагов")
    println(s"Финальное состояние: ${state.term}")
    state.term
  }
  
  def step(state: KamState): KamState = {
    state match {
      case KamState(term, instruction :: restCode, stack) =>
        instruction match {
          case Push =>
            // <: проталкивает терм в стек
            KamState(KamEmpty, restCode, term :: stack)
            
          case Swap =>
            // ,: меняет местами терм и вершину стека
            stack match {
              case head :: tail =>
                KamState(head, restCode, term :: tail)
              case _ =>
                throw new RuntimeException("Swap on empty stack")
            }
            
          case Cons =>
            // >: создает пары
            stack match {
              case head :: tail =>
                KamState(KamPair(head, term), restCode, tail)
              case _ =>
                throw new RuntimeException("Cons on empty stack")
            }
            
          case Car =>
            // Fst: первый элемент пары
            term match {
              case KamPair(fst, _) =>
                KamState(fst, restCode, stack)
              case _ =>
                throw new RuntimeException(s"Car on non-pair term: $term")
            }
            
          case Cdr =>
            // Snd: второй элемент пары
            term match {
              case KamPair(_, snd) =>
                KamState(snd, restCode, stack)
              case _ =>
                throw new RuntimeException(s"Cdr on non-pair term: $term")
            }
            
          case Cur(c) =>
            // Λ: создает замыкание
            KamState(KamClosure(c, KamEmpty), restCode, stack)
            
          case Quote(value) =>
            // ': создает константу
            KamState(KamConstant(value), restCode, stack)
            
          case App =>
            term match {
              case KamClosure(code, env) =>
                stack match {
                  case arg :: tail =>
                    code match {
                      case Cur(innerCode) :: _ =>
                        KamState(KamClosure(innerCode, KamPair(env, arg)), restCode, tail)
                      case _ =>
                        KamState(KamPair(env, arg), code, tail)
                    }
                  case _ => throw new RuntimeException("App: no argument on stack")
                }
              case _ => throw new RuntimeException(s"App on non-closure term: $term")
            }
        }
        
      case KamState(term, Nil, _) =>
        state
    }
  }

  def makePair(mCode: Code, nCode: Code): Code = 
    Push :: mCode ::: Swap :: nCode ::: Cons :: Nil
  
  def makeApp(mCode: Code, nCode: Code): Code = 
    nCode ::: Push :: mCode ::: List(App)
  
  def makeLambda(bodyCode: Code): Code = 
    List(Cur(bodyCode))
  
  def makeConstant(value: Any): Code = 
    List(Quote(value))

  val example1: Code = {
    val one = makeConstant(1)
    val two = makeConstant(2)
    val pair = makePair(one, two)
    pair ::: List(Car)
  }
  
  val example2: Code = {
    val identity = makeLambda(List(Cdr)) 
    val five = makeConstant(5)
    makeApp(identity, five)
  }
  
  val example3: Code = {
    val constantFunc = makeLambda(List(Quote(42)))
    val five = makeConstant(5)
    makeApp(constantFunc, five)
  }
  
  val example4: Code = {
    val one = makeConstant(1)
    val two = makeConstant(2)
    val three = makeConstant(3)
    
    val innerPair = makePair(one, two) ::: List(Car)
    val outerPair = makePair(innerPair, three)
    outerPair ::: List(Cdr)
  }
  
  val example5: Code = {
    val kCombinator = makeLambda(
      makeLambda(List(Car, Cdr))  
    )
    val one = makeConstant(1)
    val two = makeConstant(2)
    makeApp(makeApp(kCombinator, one), two)
  }
  
  val example6: Code = {
    val identity = makeLambda(List(Cdr))
    val applyFunc = makeLambda(List(Cdr, Push, Quote(3), Swap, App))
    makeApp(applyFunc, identity)
  }

  
  val example8: Code = {
    val identity = makeLambda(List(Cdr))
    val five = makeConstant(5)
    val six = makeConstant(6)
    
    val app = makeApp(identity, five)
    val pair = makePair(app, six)
    pair ::: List(Car)
  }
  
  def runExample(name: String, code: Code, expected: String): Unit = {
    val initialState = KamState(KamEmpty, code, Nil)
    try {
      val result = execute(initialState)
      println(s"Результат: $result (ожидается: $expected)")
    } catch {
      case e: Exception =>
        println(s"Ошибка: ${e.getMessage}")
    }
    println()
  }
  
  def runAllExamples(): Unit = {
    runExample("Пример 1: Fst <1, 2>", example1, "KamConstant(1)")
    runExample("Пример 2: (λx.x) 5", example2, "KamConstant(5)") 
    runExample("Пример 3: (λx.42) 5", example3, "KamConstant(42)")
    runExample("Пример 4: Snd <Fst <1, 2>, 3>", example4, "KamConstant(3)")
    runExample("Пример 5: (λxy.x) 1 2", example5, "KamConstant(1)")
    runExample("Пример 6: (λf.f 3)(λx.x)", example6, "KamConstant(3)")
    runExample("Пример 8: Fst <(λx.x) 5, 6>", example8, "KamConstant(5)")
  }
}

// Пример использования
object KamExample extends App {
  println("=" * 60)
  
  KamMachine.runAllExamples()
}