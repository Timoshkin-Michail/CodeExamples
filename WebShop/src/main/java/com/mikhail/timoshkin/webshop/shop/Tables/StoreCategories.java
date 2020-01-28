package com.mikhail.timoshkin.webshop.shop.Tables;

import com.mikhail.timoshkin.webshop.shop.Tables.RedisHash.Category;
import org.springframework.data.repository.CrudRepository;
import org.springframework.stereotype.Repository;

import java.util.List;

@Repository
public interface StoreCategories extends CrudRepository<Category, String> {

}
