class Main
  @a="init a"
  @b="init b"
  @c="init c"
  @d="init d"

  def run()
    PPR{
      sleep(1)
      @a=0
    }
    PPR{
      sleep(1)
      @b=1
    }
    PPR{
      sleep(1)
      @c=2
    }
    PPR{
      sleep(1)
      @d="wat"
    }
    PPR.over
    return self
  end

  def pl()
    puts @a
    puts @b
    puts @c
    puts @d

  end
end
Main.new().run().pl()
