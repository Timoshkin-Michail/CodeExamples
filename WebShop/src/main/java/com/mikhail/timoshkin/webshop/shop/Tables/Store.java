package com.mikhail.timoshkin.webshop.shop.Tables;

import com.mikhail.timoshkin.webshop.shop.Tables.RedisHash.Item;
import org.springframework.data.repository.CrudRepository;
import org.springframework.stereotype.Repository;

@Repository
public interface Store extends CrudRepository<Item, Integer> {}
